#include "code.h"

namespace Interpretation {

Stream::Output& operator<<(Stream::Output& out, const ExecBytecodeInfo& bc)
{
    using namespace Stream;
    Stream::Indented out2(out, 2);
    Stream::Indented out4(out, 4);

    out << "ExecBytecodeInfo {" << endl;
    out2 << "untypedSlotCount: " << bc.untypedSlotCount << endl
         << "frameSize: " << bc.frameSize << endl
         << "GCMap {" << endl;

    for (auto& entry : bc.referenceInfos) {
        out4 << "rtPos: " << entry.rewrittenPos << ", regMask: " << entry.regMask << ", ";
        Std::Vector::Print(out4, entry.refSlotOffsets);
        out4 << endl;
    }

    out2 << "}" << endl;
    return out << "}" << endl;
}

} // namespace Interpretation
