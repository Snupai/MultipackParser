/**
 * @file RigidFrame.h
 * @brief Container whose height does not depend on its width
 */
#ifndef MULTIPACK_UI_RIGIDFRAME_H
#define MULTIPACK_UI_RIGIDFRAME_H

#include <QFrame>

namespace multipack {
namespace ui {

/**
 * @class RigidFrame
 * @brief QFrame that keeps word-wrapped labels inside it from driving the page layout
 *
 * A word-wrapped QLabel makes every container around it "height for width".
 * A parent layout then takes such a container's preferred height as its
 * minimum, and squeezes all rows alike when the page gets less room (UR20
 * notice bar). Inside a RigidFrame the labels still wrap; to the outside the
 * frame has an ordinary size hint and minimum.
 */
class RigidFrame : public QFrame
{
public:
    using QFrame::QFrame;

    bool hasHeightForWidth() const override { return false; }
};

} // namespace ui
} // namespace multipack

#endif // MULTIPACK_UI_RIGIDFRAME_H
