#include "notify_logger_sink.h"
#include "imgui/imgui.h"
#include "imgui_notify/imgui_notify.h"

namespace satdump
{
    NotifyLoggerSink::NotifyLoggerSink()
    {
    }

    NotifyLoggerSink::~NotifyLoggerSink()
    {
    }

    void NotifyLoggerSink::receive(slog::LogMsg log)
    {
        if (log.lvl == slog::LOG_WARN)
            ImGui::InsertNotification(ImGuiToast(ImGuiToastType_Warning, "Warning", log.str.c_str()));
        else if (log.lvl == slog::LOG_ERROR)
            ImGui::InsertNotification(ImGuiToast(ImGuiToastType_Error, "Error", log.str.c_str()));
    }
}