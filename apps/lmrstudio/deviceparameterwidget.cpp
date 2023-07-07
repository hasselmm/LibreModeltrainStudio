#include "deviceparameterwidget.h"

#include <lmrs/core/algorithms.h>
#include <lmrs/core/device.h>
#include <lmrs/core/logging.h>
#include <lmrs/core/parameters.h>
#include <lmrs/core/propertyguard.h>
#include <lmrs/core/typetraits.h>
#include <lmrs/core/userliterals.h>

#include <lmrs/gui/fontawesome.h>

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QFormLayout>
#include <QHostAddress>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QValidator>

namespace lmrs::studio {

namespace {

class IPv4AddressValidator : public QRegularExpressionValidator
{
public:
    explicit IPv4AddressValidator(QObject *parent = nullptr);
};


IPv4AddressValidator::IPv4AddressValidator(QObject *parent)
    : QRegularExpressionValidator{parent}
{
    static const auto octet = R"((?:[0-1]?[0-9]?[0-9]|2[0-4][0-9]|25[0-5]))"_refrag;
    static const auto dot = "\\."_refrag;

    setRegularExpression("^"_refrag + octet + dot + octet + dot + octet + dot + octet + "$"_refrag);
}

} // namespace

class DeviceParameterWidget::Private : public core::PrivateObject<DeviceParameterWidget>
{
public:
    struct Editor
    {
        QWidget *widget;

        std::function<bool()> hasAcceptableInput;
        std::function<void(QVariant)> setValue;
        std::function<QVariant()> value;
    };

    explicit Private(core::DeviceFactory *factory, DeviceParameterWidget *d)
        : PrivateObject{d}
        , factory{factory}
        , parameters{factory->parameters()}
    {}

    Editor createEditor(core::Parameter parameter);
    Editor createChoiceEditor(QVariant model);
    Editor createFlagEditor(QVariant model);
    Editor createHostAddressEditor(QVariant model);

    void revalidate();

    QPointer<core::DeviceFactory> const factory;
    const QList<core::Parameter> parameters;
    QMap<QByteArray, Editor> editors;
    bool hasAcceptableInput = true;
};

DeviceParameterWidget::Private::Editor DeviceParameterWidget::Private::createEditor(core::Parameter parameter)
{
    switch (parameter.type()) {
    case core::Parameter::Type::Choice:
        return createChoiceEditor(parameter.model());

    case core::Parameter::Type::Flag:
        return createFlagEditor(parameter.model());

    case core::Parameter::Type::HostAddress:
        return createHostAddressEditor(parameter.model());

    case core::Parameter::Type::Number:
    case core::Parameter::Type::Text:
        LMRS_UNIMPLEMENTED_FOR_KEY(parameter.type());
        break;

    case core::Parameter::Type::Invalid:
        break;
    }

    Q_UNREACHABLE();
    return {};
}

DeviceParameterWidget::Private::Editor DeviceParameterWidget::Private::createChoiceEditor(QVariant model)
{
    const auto editor = new QComboBox{q()};

    if (const auto choices = model.toStringList(); !choices.isEmpty()) {
        for (const auto &text: choices)
            editor->addItem(text, text);
    } else if (const auto choiceModel = model.value<core::parameters::ChoiceModel>(); !choiceModel.choices.isEmpty()) {
        for (const auto &choice: choiceModel.choices)
            editor->addItem(choice.text, choice.value);
    } else {
        qCWarning(logger(), "Unsupported type %s for values of choices parameter", model.typeName());
    }

    auto value = [editor]() -> QVariant { return editor->currentData(); };
    auto setValue = [editor](QVariant value) { editor->setCurrentIndex(editor->findData(value)); };
    auto hasAcceptableInput = [editor] { return editor->currentIndex() >= 0; };

    return {editor, std::move(hasAcceptableInput), std::move(setValue), std::move(value)};
}

DeviceParameterWidget::Private::Editor DeviceParameterWidget::Private::createFlagEditor(QVariant model)
{
    if (model.typeId() != qMetaTypeId<bool>())
        qCWarning(logger(), "Unsupported type %s for boolean flag parameter", model.typeName());

    const auto editor = new QCheckBox{q()};
    editor->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    editor->setChecked(model.toBool());

    auto value = [editor]() -> QVariant { return editor->isChecked(); };
    auto setValue = [editor](QVariant value) { editor->setChecked(value.toBool()); };
    auto hasAcceptableInput = [] { return true; };

    return {editor, std::move(hasAcceptableInput), std::move(setValue), std::move(value)};
}

DeviceParameterWidget::Private::Editor DeviceParameterWidget::Private::createHostAddressEditor(QVariant model)
{
    const auto editor = new QLineEdit{q()};

    editor->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    editor->setValidator(new IPv4AddressValidator{editor});

    if (model.typeId() == qMetaTypeId<QString>()) {
        editor->setText(model.toString());
    } else if (model.typeId() == qMetaTypeId<QList<QHostAddress>>()) {
        if (auto addresses = qvariant_cast<QList<QHostAddress>>(model); !addresses.isEmpty()) {
            editor->setText(addresses.first().toString());
            editor->setCompleter(new QCompleter{core::toStringList(std::move(addresses)), editor});
        }
    } else if (model.typeId() == qMetaTypeId<QList<QStringList>>()) {
        if (auto addresses = model.toStringList(); !addresses.isEmpty()) {
            editor->setText(addresses.first());
            editor->setCompleter(new QCompleter{std::move(addresses), editor});
        }
    } else {
        qCWarning(logger(), "Unsupported type %s for host address parameter", model.typeName());
    }

    connect(editor, &QLineEdit::returnPressed, q(), &DeviceParameterWidget::activated);
    connect(editor, &QLineEdit::textChanged, this, &Private::revalidate);

    if (editor->completer()) {
        const auto showCompletionsAction = editor->addAction(icon(gui::fontawesome::fasNetworkWired),
                                                             QLineEdit::ActionPosition::TrailingPosition);

        connect(showCompletionsAction, &QAction::triggered, editor, [editor] {
            editor->completer()->complete();
        });
    }

    auto value = [editor]() -> QVariant { return editor->text(); };
    auto setValue = [editor](QVariant value) { editor->setText(value.toString()); };
    auto hasAcceptableInput = [editor] { return editor->hasAcceptableInput(); };

    return {editor, std::move(hasAcceptableInput), std::move(setValue), std::move(value)};
}

void DeviceParameterWidget::Private::revalidate()
{
    const auto guard = core::propertyGuard(q(), &DeviceParameterWidget::hasAcceptableInput,
                                           &DeviceParameterWidget::hasAcceptableInputChanged);

    hasAcceptableInput = [this] {
        for (const auto &editor: std::as_const(editors)) {
            if (!editor.hasAcceptableInput())
                return false;
        }

        return true;
    }();
}

DeviceParameterWidget::DeviceParameterWidget(core::DeviceFactory *factory, QWidget *parent)
    : QWidget{parent}
    , d{new Private{factory, this}}
{
    const auto layout = new QFormLayout{this};
    layout->setContentsMargins({});

    for (const auto &parameter: std::as_const(d->parameters)) {
        if (auto editor = d->createEditor(parameter); editor.widget) {
            auto label = new l10n::Facade<QLabel>{parameter.name(), this};
            layout->addRow(label, editor.widget);
            label->setBuddy(editor.widget);
            d->editors.insert(parameter.key(), std::move(editor));
        } else {
            qCWarning(core::logger(this), "Unsupported type %d for parameter \"%s\"",
                      core::value(parameter.type()), parameter.key().constData());
        }
    }

    d->revalidate();
}

core::DeviceFactory *DeviceParameterWidget::factory() const
{
    return d->factory;
}

void DeviceParameterWidget::setParameters(QVariantMap parameters)
{
    for (auto it = parameters.begin(); it != parameters.end(); ++it) {
        if (const auto setValue = d->editors[it.key().toLatin1()].setValue)
            setValue(it.value());
    }
}

QVariantMap DeviceParameterWidget::parameters() const
{
    auto parameters = QVariantMap{};

    for (auto it = d->editors.begin(); it != d->editors.end(); ++it) {
        if (it->value)
            parameters.insert(QString::fromLatin1(it.key()), it->value());
    }

    return parameters;
}

bool DeviceParameterWidget::hasAcceptableInput() const
{
    return d->hasAcceptableInput;
}

} // namespace lmrs::studio
