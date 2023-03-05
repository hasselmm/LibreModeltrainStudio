#include "z21appfileformats.h"

#include <lmrs/core/logging.h>
#include <lmrs/core/symbolictrackplanmodel.h>
#include <lmrs/core/userliterals.h>

#include <QtGui/private/qzipreader_p.h>

#include <QElapsedTimer>
#include <QRect>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTemporaryFile>
#include <QUuid>

namespace lmrs::roco::z21app {

namespace {

using core::TrackSymbol;
using core::TrackSymbolInstance;

constexpr auto s_binding_pageId = ":pageId"_L1;

constexpr auto s_column_address = "address"_L1;
constexpr auto s_column_address1 = "address1"_L1;
constexpr auto s_column_address2 = "address2"_L1;
constexpr auto s_column_afterglow = "afterglow"_L1;
constexpr auto s_column_angle = "angle"_L1;
constexpr auto s_column_fontSize = "font_size"_L1;
constexpr auto s_column_id = "id"_L1;
constexpr auto s_column_max_x = "max_x"_L1;
constexpr auto s_column_max_y = "max_y"_L1;
constexpr auto s_column_min_x = "min_x"_L1;
constexpr auto s_column_min_y = "min_y"_L1;
constexpr auto s_column_name = "name"_L1;
constexpr auto s_column_reportAddress = "report_address"_L1;
constexpr auto s_column_text = "text"_L1;
constexpr auto s_column_type = "type"_L1;
constexpr auto s_column_x = "x"_L1;
constexpr auto s_column_y = "y"_L1;

constexpr auto s_query_layoutData = R"(
    SELECT * FROM layout_data
)"_L1;

constexpr auto s_query_pages = R"(
    SELECT * FROM control_station_pages
    ORDER BY position
)"_L1;

constexpr auto s_query_pageLimits = R"(
    SELECT min(min_x) AS min_x, min(min_y) AS min_y,
           max(max_x) AS max_x, max(max_y) AS max_y
    FROM (
        SELECT min(x) AS min_x, min(y) AS min_y,
               max(x) AS max_x, max(y) AS max_y
          FROM control_station_controls
         WHERE page_id = :pageId
    UNION
        SELECT min(x) AS min_x, min(y) AS min_y,
               max(x) AS max_x, max(y) AS max_y
          FROM control_station_notes
         WHERE page_id = :pageId
    UNION
        SELECT min(x) AS min_x, min(y) AS min_y,
               max(x) AS max_x, max(y) AS max_y
          FROM control_station_response_modules
         WHERE page_id = :pageId
    UNION
        SELECT min(x) AS min_x, min(y) AS min_y,
               max(x) AS max_x, max(y) AS max_y
          FROM control_station_routes
         WHERE page_id = :pageId
    UNION
        SELECT min(x) AS min_x, min(y) AS min_y,
               max(x) AS max_x, max(y) AS max_y
          FROM control_station_signals
         WHERE page_id = :pageId
    )
)"_L1;

constexpr auto s_query_controls = R"(
    SELECT * FROM control_station_controls
    WHERE page_id = :pageId
)"_L1;

constexpr auto s_query_notes = R"(
    SELECT * FROM control_station_notes
    WHERE page_id = :pageId
)"_L1;

constexpr auto s_query_responseModules = R"(
    SELECT * FROM control_station_response_modules
    WHERE page_id = :pageId
)"_L1;

class AutoTimer : public QElapsedTimer
{
public:
    explicit AutoTimer(const char *label)
        : m_label(label)
    {
        start();
    }

    ~AutoTimer()
    {
        report();
    }

    void report() const
    {
        qInfo() << m_label << std::chrono::milliseconds{elapsed()};
    }

private:
    const char *const m_label;
};

auto findFile(const QZipReader &reader, QString fileName)
{
    constexpr auto pathSeparator = '/'_L1;
    constexpr auto prefix = "export/"_L1;
    const auto suffix = pathSeparator + fileName;

    for (const auto &entry: reader.fileInfoList()) {
        if (entry.isFile
                && entry.filePath.endsWith(suffix)
                && entry.filePath.startsWith(prefix)
                && entry.filePath.indexOf(pathSeparator, prefix.size()) == entry.filePath.size() - suffix.size())
            return entry;
    }

    return QZipReader::FileInfo{};
}

template<class T>
struct DatabaseContext
{
    LMRS_CORE_DEFINE_LOGGER(T)

    QSqlDatabase database;

    auto runQuery(const char *queryName, QString queryString, QVariantMap bindings);
};

template<class T>
auto DatabaseContext<T>::runQuery(const char *queryName, QString queryString, QVariantMap bindings)
{
    auto query = QSqlQuery{database};

    if (!query.prepare(std::move(queryString))) {
        qCWarning(logger(), "Could not prepare %s query: %ls", queryName, qUtf16Printable(query.lastError().text()));
        return QSqlQuery{};
    }

    for (auto [key, value]: bindings.asKeyValueRange())
        query.bindValue(std::move(key), std::move(value));

    if (!query.exec()) {
        qCWarning(logger(), "Could not run %s query: %ls", queryName, qUtf16Printable(query.lastError().text()));
        return QSqlQuery{};
    }

    return query;
}

struct LayoutReaderContext : DatabaseContext<LayoutReader>
{
    std::unique_ptr<core::SymbolicTrackPlanLayout> layout;

    explicit LayoutReaderContext(QString fileName, QString connectionName);
    ~LayoutReaderContext();

    bool open();
    bool readPages();
};

struct PageReaderContext : DatabaseContext<LayoutReader>
{
    std::unique_ptr<core::SymbolicTrackPlanModel> page;

    const int pageId;
    QRect pageBounds;

    explicit PageReaderContext(QSqlDatabase database, QSqlRecord record);

    auto typeAt(int row, int column) const;

    bool readPageBounds();
    bool readControls();
    bool readNotes();
    bool readResponseModules();
    bool readRoutes();
    bool readSignals();
};

LayoutReaderContext::LayoutReaderContext(QString fileName, QString connectionName)
    : DatabaseContext{QSqlDatabase::addDatabase("QSQLITE"_L1, std::move(connectionName))}
{
    database.setDatabaseName(std::move(fileName));
}

LayoutReaderContext::~LayoutReaderContext()
{
    auto connectionName = database.connectionName();

    database.close();
    database = {};

    QSqlDatabase::removeDatabase(std::move(connectionName));
}

bool LayoutReaderContext::open()
{
    if (!database.open()) {
        qCWarning(logger(), "Could not open the database: %ls", qUtf16Printable(database.lastError().text()));
        return false;
    }

    auto query = runQuery("layout data", s_query_layoutData, {});

    if (!query.isActive())
        return false;

    if (!query.next()) {
        qCWarning(logger(), "No layout data found");
        return false;
    }

    layout = std::make_unique<core::SymbolicTrackPlanLayout>();
    layout->name = query.value(s_column_name).toString();

    return true;
}

bool LayoutReaderContext::readPages()
{
    auto query = runQuery("pages", s_query_pages, {});

    if (!query.isActive())
        return false;

    while (query.next()) {
        auto pageContext = PageReaderContext{database, query.record()};

        if (!pageContext.readPageBounds()
                || !pageContext.readControls()
                || !pageContext.readNotes()
                || !pageContext.readResponseModules()
                || !pageContext.readRoutes()
                || !pageContext.readSignals())
            return false;

        layout->pages.emplace_back(std::move(pageContext.page));
    }

    return true;
}

PageReaderContext::PageReaderContext(QSqlDatabase database, QSqlRecord record)
    : DatabaseContext{std::move(database)}
    , page{std::make_unique<core::SymbolicTrackPlanModel>()} // FIXME: set parent?
    , pageId(record.value(s_column_id).toInt())
{
    qCDebug(logger()) << "reading page" << pageId << page->title();
    page->setTitle(record.value(s_column_name).toString());
}

auto PageReaderContext::typeAt(int row, int column) const
{
    return qvariant_cast<TrackSymbol::Type>(page->index(row, column).data(page->TypeRole));
}

bool PageReaderContext::readPageBounds()
{
    const auto stopWatch = AutoTimer{__func__};
    auto query = runQuery("page limits", s_query_pageLimits, {{s_binding_pageId, pageId}});

    if (!query.isActive())
        return false;

    if (query.next()) {
        const auto max_x = query.value(s_column_max_x).toInt();
        const auto max_y = query.value(s_column_max_y).toInt();
        const auto min_x = query.value(s_column_min_x).toInt();
        const auto min_y = query.value(s_column_min_y).toInt();

        pageBounds = {QPoint{min_x, min_y}, QPoint{max_x, max_y}};
        pageBounds += QMargins{1, 1, 1, 1};
    } else {
        pageBounds = {};
    }

    qCDebug(logger()) << "with boundaries" << pageBounds;
    page->resize(pageBounds.size());

    return true;
}

bool PageReaderContext::readControls()
{
    using Type = TrackSymbol::Type;

    const auto stopWatch = AutoTimer{__func__};
    auto query = runQuery("controls", s_query_controls, {{s_binding_pageId, pageId}});

    if (!query.isActive())
        return false;

    while (query.next()) {
        const auto id = query.value(s_column_id).toInt();
        const auto x = query.value(s_column_x).toInt();
        const auto y = query.value(s_column_y).toInt();
        const auto angle = query.value(s_column_angle).toInt();
        const auto type = query.value(s_column_type).toInt();
        const auto address1 = query.value(s_column_address1);
        const auto address2 = query.value(s_column_address2);

        //                qInfo() << controls.value("id"_L1).toInt()
        //                        << x << y << angle << type
        //                        << controls.value("address1"_L1).toInt()
        //                        << controls.value("address2"_L1).toInt()
        //                        << controls.value("address3"_L1).toInt()
        //                        << controls.value("button_type"_L1).toInt()
        //                        << controls.value("time"_L1).toInt()
        //                        << firstPage;

        static const auto typeMap = QHash<int, Type> {
            {0,  Type::Straight},
            {1,  Type::RightHandPoint},
            {2,  Type::LeftHandPoint},
            {3,  Type::DoubleCrossPoint},
            {4,  Type::ThreeWayPoint},
            // 5: CurvedLeft
            // 6: CurvedRight
            {7,  Type::YPoint},
            {8,  Type::Decoupler},
            {9,  Type::SingleCrossPoint},
            {10, Type::Light},
            {11, Type::TwoLightsSignal},
            {12, Type::ThreeLightsSignal},
            {13, Type::FourLightsSignal},
            {14, Type::Terminal},
            {17, Type::ShuntingSignal},
            {18, Type::SemaphoreSignal},
            {20, Type::Switch},
            {23, Type::DoubleCrossPoint},
            {24, Type::NarrowCurve},
            {26, Type::Button},
            {27, Type::Trees},
            {28, Type::WideCurve},
            {29, Type::Platform},
            {30, Type::RedRoof},
            {31, Type::BlackRoof},
            {32, Type::BrownRoof},
            {33, Type::BridgeStraight},
            {34, Type::BridgeEntry},
            {36, Type::LevelCrossing},
            {37, Type::TunnelCurve},
            {38, Type::TunnelEntry},
            {39, Type::TunnelStraight},
            {42, Type::Cross},

        };

        auto cell = TrackSymbolInstance{};
        cell.symbol.type = typeMap.value(type, Type::Empty);
        cell.symbol.state = defaultState(cell.symbol.type);
        cell.orientation = TrackSymbolInstance::Orientation{angle};

        switch (cell.symbol.type) {
        case Type::Straight:
        case Type::NarrowCurve:
        case Type::WideCurve:
        case Type::Terminal:
        case Type::Cross:
        case Type::LevelCrossing:
        case Type::BridgeEntry:
        case Type::BridgeStraight:
        case Type::TunnelEntry:
        case Type::TunnelStraight:
        case Type::TunnelCurve:
        case Type::Platform:
        case Type::RedRoof:
        case Type::BlackRoof:
        case Type::BrownRoof:
        case Type::Trees:
            break;

        case Type::LeftHandPoint:
        case Type::RightHandPoint:
        case Type::YPoint:
        case Type::SingleCrossPoint:
        case Type::DoubleCrossPointSimple:
        case Type::TwoLightsSignal:
        case Type::ShuntingSignal:
        case Type::SemaphoreSignal:
        case Type::Button:
        case Type::Decoupler:
        case Type::Light:
        case Type::Switch:
            cell.detail = address1.toInt();
            break;

        case Type::ThreeWayPoint:
        case Type::DoubleCrossPoint:
        case Type::ThreeLightsSignal:
        case Type::FourLightsSignal:
            cell.detail = QVariant::fromValue(QList{address1.toInt(), address2.toInt()});
            break;

        case Type::Detector:
        case Type::Text:
            Q_UNREACHABLE();
            break;

        case Type::Empty:
            switch (type) {
            case 40:
                cell = TrackSymbolInstance{"Signal\n(DCCext)"_L1};
                break;

            default:
                qCWarning(logger(),
                          "Unsupported control #%d of type #%d at x=%d, y=%d of page \"%ls\"",
                          id, type, x, y, qUtf16Printable(page->title()));

                cell = TrackSymbolInstance{"Control\ntype="_L1 + QString::number(type)};
                break;
            }

            break;
        }

        auto index = page->index(y - pageBounds.top(), x - pageBounds.left());
        const auto success = page->setData(std::move(index), std::move(cell));

        if (!success)
            qCWarning(logger(), "Could not insert symbol at x=%d, y=%d", x, y);
    }

    for (auto row = 0, rowCount = page->rowCount(); row < rowCount; ++row) {
        for (auto column = 0, columnCount = page->columnCount(); column < columnCount; ++column) {
            const auto type = typeAt(row, column);

            if (!hasLinks(type))
                continue;

            auto links = TrackSymbol::Links{TrackSymbol::Link::Center};

            if (const auto north = row - 1; north >= 0 && typeAt(north, column) == type)
                links |= TrackSymbol::Link::ConnectNorth;
            if (const auto south = row + 1; south < rowCount && typeAt(south, column) == type)
                links |= TrackSymbol::Link::ConnectSouth;
            if (const auto west = column - 1; west >= 0 && typeAt(row, west) == type)
                links |= TrackSymbol::Link::ConnectWest;
            if (const auto east = column + 1; east < columnCount && typeAt(row, east) == type)
                links |= TrackSymbol::Link::ConnectEast;

            const auto orientation
                    = links == TrackSymbol::Link::Vertical
                    ? TrackSymbolInstance::Orientation::West
                    : TrackSymbolInstance::Orientation::North;

            if (!page->setData(page->index(row, column), QVariant::fromValue(links), page->LinksRole))
                qCWarning(logger(), "Could not link symbol at x=%d, y=%d", column, row);
            if (!page->setData(page->index(row, column), QVariant::fromValue(orientation), page->OrientationRole))
                qCWarning(logger(), "Could not rotate symbol at x=%d, y=%d", column, row);
        }
    }

    return true;
}

bool PageReaderContext::readNotes()
{
    using Type = TrackSymbol::Type;

    const auto stopWatch = AutoTimer{__func__};
    auto query = runQuery("notes", s_query_notes, {{s_binding_pageId, pageId}});

    if (!query.isActive())
        return false;

    while (query.next()) {
        const auto id = query.value(s_column_id).toInt();
        const auto x = query.value(s_column_x).toInt();
        const auto y = query.value(s_column_y).toInt();
        const auto angle = query.value(s_column_angle).toInt();
        const auto type = query.value(s_column_type).toInt();
        const auto text = query.value(s_column_text).toString();
        const auto fontSize = query.value(s_column_fontSize).toInt();

        static const auto typeMap = QHash<int, Type> {
            {1, Type::Text},
            {2, Type::Straight},
        };

        auto cell = TrackSymbolInstance{};
        cell.symbol.type = typeMap.value(type, Type::Empty);
        cell.symbol.state = defaultState(cell.symbol.type);
        cell.orientation = TrackSymbolInstance::Orientation{angle};

        switch (cell.symbol.type) {
        case Type::Text:
            cell.detail = QVariantList{text, fontSize};
            break;

        case Type::Straight:
            cell.detail = text;
            break;

        case Type::NarrowCurve:
        case Type::WideCurve:
        case Type::Terminal:
        case Type::Cross:
        case Type::LevelCrossing:
        case Type::BridgeEntry:
        case Type::BridgeStraight:
        case Type::TunnelEntry:
        case Type::TunnelStraight:
        case Type::TunnelCurve:
        case Type::LeftHandPoint:
        case Type::RightHandPoint:
        case Type::YPoint:
        case Type::SingleCrossPoint:
        case Type::DoubleCrossPoint:
        case Type::DoubleCrossPointSimple:
        case Type::TwoLightsSignal:
        case Type::ShuntingSignal:
        case Type::SemaphoreSignal:
        case Type::Button:
        case Type::Decoupler:
        case Type::Light:
        case Type::Switch:
        case Type::ThreeWayPoint:
        case Type::ThreeLightsSignal:
        case Type::FourLightsSignal:
        case Type::Detector:
        case Type::Platform:
        case Type::RedRoof:
        case Type::BlackRoof:
        case Type::BrownRoof:
        case Type::Trees:
            Q_UNREACHABLE();
            break;

        case Type::Empty:
            cell = TrackSymbolInstance{"Control\ntype="_L1 + QString::number(type)};

            qCWarning(logger(),
                      "Unsupported control #%d of type #%d at x=%d, y=%d of page \"%ls\"",
                      id, type, x, y, qUtf16Printable(page->title()));

            break;
        }

        auto index = page->index(y - pageBounds.top(), x - pageBounds.left());
        const auto success = page->setData(std::move(index), std::move(cell));

        if (!success)
            qCWarning(logger(), "Could not insert symbol at x=%d, y=%d", x, y);
    }

    return true;
}

bool PageReaderContext::readResponseModules()
{
    const auto stopWatch = AutoTimer{__func__};
    auto query = runQuery("response modules", s_query_responseModules, {{s_binding_pageId, pageId}});

    if (!query.isActive())
        return false;

    while (query.next()) {
        struct ResponseModule
        {
            enum class Type
            {
                Uhlenbrock63320 = 0,
                UhlenbrockLissy = 1,
                Bluecher = 2,
                Roco10787 = 3,
                Roco10808 = 4,
                Roco10819 = 5,
            };

            enum class Protocol
            {
                RBus,
                Loconet,
                CAN,
            };

            Type type;
            int address;
            int aspect;
            int afterglow;

            static constexpr auto protocol(Type) -> Protocol;
            static constexpr auto addressRange(Type) -> core::Range<int>;
            static constexpr auto aspectRange(Type) -> core::Range<int>;
            static constexpr auto afterglowRange(Type) -> core::Range<int>;
        };

//        const auto id = query.value(s_column_id).toInt();
        const auto x = query.value(s_column_x).toInt();
        const auto y = query.value(s_column_y).toInt();
        const auto angle = query.value(s_column_angle).toInt();

        auto settings = ResponseModule {
            ResponseModule::Type{query.value(s_column_type).toInt()},
            query.value(s_column_address).toInt(),
            query.value(s_column_reportAddress).toInt(),
            query.value(s_column_afterglow).toInt(),
        };

        // FIXME: store ResponseModule details

        // Type | Product Name          | Protocol  | Address   | Aspect    | Afterglow
        //======================================================================================================
        // 3    | Roco 10787            | R-BUS     | 1-20      | Port 1-8  | 0-9999
        // 2    | Blücher GBM16XL/XN    | LocoNet   | 1-2048    | -         |
        // 0    | Uhlenbrock 63320      | LocoNet   | 1-2048    | Report Address = 1017 (1-2048)
        // 1    | Uhlenbrock Lissy      | LocoNet   | 1-4095    |           |
        // 4    | Roco 10808            | CAN       | 1-256     | Port 1-8  |
        // 5    | Roco 10819            | R-BUS     | 1-19      | Port 1-16 | 0-9999

        qInfo() << "RM" << x << y << angle << core::value(settings.type);

        auto cell = TrackSymbolInstance{};
        cell.symbol.type = TrackSymbol::Type::Detector;
        cell.symbol.state = defaultState(cell.symbol.type);
        cell.orientation = TrackSymbolInstance::Orientation{angle};
        cell.detail = QVariant::fromValue(QList<int>{settings.address, settings.aspect});

        auto index = page->index(y - pageBounds.top(), x - pageBounds.left());
        const auto success = page->setData(std::move(index), std::move(cell));

        if (!success)
            qCWarning(logger(), "Could not insert symbol at x=%d, y=%d", x, y);
    }

    LMRS_UNIMPLEMENTED();
    return true;
}

bool PageReaderContext::readRoutes()
{
    const auto stopWatch = AutoTimer{__func__};

    LMRS_UNIMPLEMENTED();
    return true;
}

bool PageReaderContext::readSignals()
{
    const auto stopWatch = AutoTimer{__func__};

    LMRS_UNIMPLEMENTED();
    return true;
}

} // namespace

LayoutReader::ModelPointer LayoutReader::read()
{
    reset();

    const auto stopWatch = AutoTimer{__func__};

    auto reader = QZipReader{fileName()};
    const auto info = findFile(reader, "Loco.sqlite"_L1);

    if (info.filePath.isEmpty()) {
        reportError(tr("No supported layout database found in this file."));
        return {};
    }

    const auto data = reader.fileData(info.filePath);

    if (data.size() != info.size) {
        reportError(tr("Extracting the layout database has failed."));
        return {};
    }

    auto databaseFile = QTemporaryFile{};

    if (!databaseFile.open()) {
        reportError(databaseFile.errorString());
        return {};
    }

    if (databaseFile.write(data) != data.size()) {
        reportError(databaseFile.errorString());
        return {};
    }

    databaseFile.close();

    auto layoutContext = LayoutReaderContext{databaseFile.fileName(), QUuid::createUuid().toString()};

    if (!layoutContext.open()) {
        reportError(tr("Opening the layout database has failed."));
        return {};
    }

    if (!layoutContext.readPages()) {
        reportError(tr("The layout database has an unsupported format."));
        return {};
    }

    qInfo() << layoutContext.layout->name << layoutContext.layout->pages.size();
    return std::move(layoutContext.layout);
}

bool LayoutWriter::write(const core::SymbolicTrackPlanLayout *)
{
    reportError(tr("Not implemented yet"));
    return false;
}

} // namespace lmrs::roco::z21app
