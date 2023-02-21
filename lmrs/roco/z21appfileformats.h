#ifndef LMRS_ROCO_Z21APP_FILEFORMATS_H
#define LMRS_ROCO_Z21APP_FILEFORMATS_H

#include <lmrs/core/symbolictrackplanmodel.h>

namespace lmrs::roco::z21app {

class LayoutReader : public core::SymbolicTrackPlanReader
{
    Q_GADGET
    QT_TR_FUNCTIONS

public:
    using core::SymbolicTrackPlanReader::SymbolicTrackPlanReader;
    ModelPointer read() override;
};

class LayoutWriter : public core::SymbolicTrackPlanWriter
{
    Q_GADGET
    QT_TR_FUNCTIONS

public:
    using core::SymbolicTrackPlanWriter::SymbolicTrackPlanWriter;
    bool write(const core::SymbolicTrackPlanLayout *layout) override;
};

} // namespace lmrs::roco::z21app

#endif // LMRS_ROCO_Z21APPFILEFORMATS_H
