#pragma once

#include <QString>

namespace Exteraless {

[[nodiscard]] QString StripTracking(const QString &url);
[[nodiscard]] QString StripTrackingInText(const QString &text);

} // namespace Exteraless
