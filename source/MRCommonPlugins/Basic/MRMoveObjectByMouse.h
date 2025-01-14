#pragma once
#include "MRViewer/MRStatePlugin.h"
#include "MRViewer/MRMoveObjectByMouseImpl.h"
#include "MRMesh/MRPlane3.h"
#include "MRMesh/MRAffineXf3.h"
#include "MRCommonPlugins/exports.h"
#include "imgui.h"

namespace MR
{

class Object;

class MoveObjectByMouse : public StateListenerPlugin<DragStartListener, DragListener, DragEndListener>
{
public:
    MoveObjectByMouse();
    ~MoveObjectByMouse();

    MRCOMMONPLUGINS_API static MoveObjectByMouse* instance();

    virtual bool onDisable_() override;
    virtual void drawDialog( float menuScaling, ImGuiContext* ) override;

    virtual bool blocking() const override { return false; };

private:
    virtual bool onDragStart_( MouseButton btn, int modifiers ) override;
    virtual bool onDrag_( int x, int y ) override;
    virtual bool onDragEnd_( MouseButton btn, int modifiers ) override;

    class MoveObjectByMouseWithSelected : public MoveObjectByMouseImpl
    {
    protected:
        std::vector<std::shared_ptr<Object>> getObjects_( 
            const std::shared_ptr<VisualObject>& obj, const PointOnObject& point, int modifiers ) override;
    } moveByMouse_;
};

}
