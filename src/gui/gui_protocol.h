#pragma once

// Wire protocol between the compositor (the only process that opens /dev/fb0)
// and client applications. Clients never touch the framebuffer: the compositor
// allocates a shared-memory region per window, the client maps it and draws
// pixels directly, then reports damage so the compositor knows to recomposite.
//
// Regions used by the system, all created by the compositor:
//
//   eucalypt-gui-ctl        one control block: the client slot table
//   eucalypt-gui-slot-N     one per client: request in, event out
//   eucalypt-gui-win-N      one per window: the window's backing pixels
//
// A client finds a region by name with the SHM_IOC_QUERY ioctl, which returns
// both its mmap offset and its size, so no address is ever exchanged between
// processes.

#include <stdint.h>

#define GUI_MAX_CLIENTS 8
#define GUI_MAX_WINDOWS 32
#define GUI_TITLE_LEN   32
#define GUI_CTL_MAGIC   0x47554943u  // "GUIC"

// Cursor sprite dimensions, in pixels
#define GUI_CURSOR_W 16
#define GUI_CURSOR_H 16

// Client -> compositor
enum {
    GUI_REQ_NONE          = 0,
    GUI_REQ_CREATE_WINDOW = 1,
    GUI_REQ_DESTROY_WINDOW = 2,
    GUI_REQ_MOVE_WINDOW   = 3,
    GUI_REQ_DAMAGE        = 4,
    GUI_REQ_SET_TITLE     = 5,
    GUI_REQ_QUIT          = 6,
};

typedef struct {
    uint32_t type;          // one of GUI_REQ_*
    uint32_t window_id;     // target window, or 0 where not applicable
    int32_t  x, y;          // position, or damage origin
    int32_t  w, h;          // size, or damage extent
    char     title[GUI_TITLE_LEN];
} gui_request_t;

// Compositor -> client. `status` is 0 on success, negative errno on failure.
typedef struct {
    uint32_t window_id;     // for GUI_RSP_CREATE_WINDOW
    uint64_t shm_index;     // mmap offset selector for the window's region
    uint64_t shm_size;      // size of that region in bytes
    int32_t  status;
} gui_reply_t;

// Events pushed to the client that owns the relevant window
enum {
    GUI_EVT_NONE    = 0,
    GUI_EVT_KEY     = 1,
    GUI_EVT_BUTTON  = 2,
    GUI_EVT_MOTION  = 3,
    GUI_EVT_FOCUS   = 4,
    GUI_EVT_CLOSE   = 5,   // the window manager asked this window to close
};

typedef struct {
    uint32_t type;          // one of GUI_EVT_*
    uint32_t window_id;
    int32_t  x, y;
    int32_t  button;        // button index, or key code for GUI_EVT_KEY
    int32_t  state;         // 1 pressed / focused, 0 released / blurred
} gui_event_t;

// One client's private mailbox. The client publishes a request by making
// req_seq odd, and waits for it to go even again once the compositor has filled
// in reply. Events work the same way through evt_seq. Volatile plus the sequence
// counters is enough here because the compositor and its clients run on the one
// CPU this kernel boots.
typedef struct {
    volatile uint32_t owner;    // 0 when unclaimed, otherwise the owning pid
    volatile uint32_t req_seq;
    gui_request_t    req;
    gui_reply_t      reply;
    volatile uint32_t evt_seq;
    gui_event_t      evt;
} gui_slot_t;

// The control region: a directory of slots plus the cursor's home window id.
typedef struct {
    uint32_t magic;
    uint32_t slot_count;
    uint32_t cursor_window_id;
    uint32_t reserved;
    gui_slot_t slots[GUI_MAX_CLIENTS];
} gui_ctl_t;
