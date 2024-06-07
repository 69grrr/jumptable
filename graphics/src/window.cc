#include "graphics/window.hh"

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <xcb/xcb.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_ewmh.h>
#include <memory>
#include <unistd.h>
#include <xcb/xproto.h>
#include "graphics/window_events.hh"

namespace graphics
{
    struct Window::WindowData
    {
        xcb_connection_t *connection;
        xcb_screen_t *screen;
        xcb_window_t window_id;

        xcb_atom_t intern(const char *name)
        {
            auto cookie = xcb_intern_atom(connection, 0, strlen(name), name);
            auto reply = xcb_intern_atom_reply(connection, cookie, nullptr);
            auto atom = reply->atom;
            free(reply);
            return atom;
        }

        void setProperty(const char *name, const char *value, const char *type)
        {
            xcb_change_property(
                connection,
                XCB_PROP_MODE_REPLACE,
                window_id,
                intern(name),
                intern(type),
                8,
                strlen(value),
                value
            );
        }
    };

    WindowBuilder &WindowBuilder::title(const char *title)
    {
        title_ = title;
        return *this;
    }

    WindowBuilder &WindowBuilder::size(int width, int height)
    {
        width_ = width;
        height_ = height;
        return *this;
    }

    WindowBuilder &WindowBuilder::min_size(int min_width, int min_height)
    {
        min_width_ = min_width;
        min_height_ = min_height;
        return *this;
    }

    Window WindowBuilder::build()
    {
        std::unique_ptr<Window::WindowData> data = std::make_unique<Window::WindowData>();

        data->connection = xcb_connect(nullptr, nullptr);

        const xcb_setup_t *setup = xcb_get_setup(data->connection);
        xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
        data->screen = iter.data;

        data->window_id = xcb_generate_id(data->connection);

        uint32_t mask = XCB_CW_EVENT_MASK;
        uint32_t value_list[] = {
            XCB_EVENT_MASK_EXPOSURE
        };
        xcb_create_window(
            data->connection,              /* connection */
            XCB_COPY_FROM_PARENT,          /* depth */
            data->window_id,               /* window id           */
            data->screen->root,            /* parent window       */
            0, 0,                          /* x, y                */
            width_, height_,               /* width, height       */
            10,                            /* border_width        */
            XCB_WINDOW_CLASS_INPUT_OUTPUT, /* class               */
            data->screen->root_visual,     /* visual              */
            mask,
            value_list
        );

        xcb_icccm_set_wm_name(data->connection, data->window_id,
            XCB_ATOM_STRING,
            // 8 because everyone uses 8, not sure why
            8,
            strlen(title_), title_
        );
        xcb_size_hints_t hints;
        hints.flags = XCB_ICCCM_SIZE_HINT_P_SIZE |
                 XCB_ICCCM_SIZE_HINT_P_MIN_SIZE;
        hints.width = width_;
        hints.height = height_;
        hints.min_width = min_width_;
        hints.min_height = min_height_;

        xcb_icccm_set_wm_normal_hints_checked(
            data->connection, data->window_id, &hints
        );

        xcb_map_window(data->connection, data->window_id);
        xcb_flush(data->connection);

        return Window(std::move(data));
    }

    Window::Window(std::unique_ptr<WindowData> data): data_(std::move(data))
    { }

    Window::~Window()
    {
        if (data_->connection)
            xcb_disconnect(data_->connection);
    }

    std::unique_ptr<WindowEvent> window_event_from_xcb_event(xcb_generic_event_t *event)
    {
        switch (event->response_type & ~0x80)
        {
        case XCB_EXPOSE:
            {
                xcb_expose_event_t *expose = reinterpret_cast<xcb_expose_event_t *>(event);
                return std::make_unique<WindowEventExpose>(WindowEventExpose({
                    .width = expose->width,
                    .height = expose->height,
                    .x = expose->x,
                    .y = expose->y,
                }));
            }
        default:
            return std::unique_ptr<WindowEvent>();
        }
    }

    std::unique_ptr<WindowEvent> Window::poll_event()
    {
        return window_event_from_xcb_event(xcb_poll_for_event(data_->connection));
    }

    std::unique_ptr<WindowEvent> Window::wait_event()
    {
        return window_event_from_xcb_event(xcb_wait_for_event(data_->connection));
    }
}
