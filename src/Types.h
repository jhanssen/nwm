#ifndef TYPES_H
#define TYPES_H

namespace Types
{
    enum {
        RootEventMask = XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT
                        | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY
                        | XCB_EVENT_MASK_ENTER_WINDOW
                        | XCB_EVENT_MASK_LEAVE_WINDOW
                        | XCB_EVENT_MASK_STRUCTURE_NOTIFY
                        | XCB_EVENT_MASK_BUTTON_PRESS
                        | XCB_EVENT_MASK_BUTTON_RELEASE
                        | XCB_EVENT_MASK_FOCUS_CHANGE
                        | XCB_EVENT_MASK_PROPERTY_CHANGE,
        ClientInputMask = XCB_EVENT_MASK_STRUCTURE_NOTIFY
                          | XCB_EVENT_MASK_PROPERTY_CHANGE
                          | XCB_EVENT_MASK_FOCUS_CHANGE
    };
};

#endif
