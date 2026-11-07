#include "console.h"

/**
 * @brief Append a physical line, discarding the oldest when history is full.
 * @param console Capture state whose log_mutex is already locked by the caller.
 * @param text Line bytes; length must be less than DEBUG_CONSOLE_LINE_BYTES.
 * @param length Number of text bytes, excluding the terminator.
 * @param category Original SDL category.
 * @param priority Original SDL priority.
 */
static void console_append_line_locked(DebugConsole *console, const char *text, size_t length, int category, SDL_LogPriority priority)
{
    size_t index;
    if (console->line_count < DEBUG_CONSOLE_HISTORY_LINES) {
        index = (console->first_line + console->line_count) % DEBUG_CONSOLE_HISTORY_LINES;
        ++console->line_count;
    } 
    else {
        index = console->first_line;
        console->first_line = (console->first_line + 1) % DEBUG_CONSOLE_HISTORY_LINES;
    }

    DebugConsoleLine *line = &console->lines[index];
    SDL_memcpy(line->text, text, length);
    line->text[length] = '\0';
    line->category = category;
    line->priority = priority;
}

/**
 * @brief Collect SDL output without drawing, allocating, or recursively logging.
 * @param userdata Stable DebugConsole pointer installed by debug_console_log_init.
 * @param category Original SDL category.
 * @param priority Original SDL priority.
 * @param message Null-terminated message; LF/CRLF are split and long lines hard-wrap.
 * @note May run on a logging worker thread; protects shared history with log_mutex.
 */
static void SDLCALL console_log_output(void *userdata, int category, SDL_LogPriority priority, const char *message)
{
    DebugConsole *console = userdata;
    char line[DEBUG_CONSOLE_LINE_BYTES];
    size_t length = 0;
    bool ended_with_newline = false;

    SDL_LockMutex(console->log_mutex);
    for (const char *p = message; *p; ++p) {
        if (*p == '\r' && p[1] == '\n')
            continue;
        if (*p == '\n' || *p == '\r') {
            console_append_line_locked(console, line, length, category, priority);
            length = 0;
            ended_with_newline = true;
        } else {
            if (length == sizeof(line) - 1) {
                console_append_line_locked(console, line, length, category, priority);
                length = 0;
            }
            line[length++] = *p == '\t' ? ' ' : *p;
            ended_with_newline = false;
        }
    }
    if (length > 0 || (!ended_with_newline && message[0] == '\0'))
    console_append_line_locked(console, line, length, category, priority);
    SDL_UnlockMutex(console->log_mutex);

    if (console->previous_log_output)
        console->previous_log_output(console->previous_log_userdata, category, priority, message);
}

bool debug_console_log_init(DebugConsole *console)
{
    *console = (DebugConsole){0};
    console->log_mutex = SDL_CreateMutex();
    if (!console->log_mutex)
        return false;

    SDL_GetLogOutputFunction(&console->previous_log_output, &console->previous_log_userdata);
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_TRACE);
    SDL_SetLogPriority(SDL_LOG_CATEGORY_INPUT, SDL_LOG_PRIORITY_INFO);
    console->log_output_installed = true;
    SDL_SetLogOutputFunction(console_log_output, console);
    return true;
}

bool debug_console_init(DebugConsole *console)
{
    return SDL_CreateWindowAndRenderer("littleEngine Console", 800, 400, SDL_WINDOW_RESIZABLE, &console->window, &console->renderer);
}

void debug_console_shutdown(DebugConsole *console)
{
    if (!console)
        return;
    if (console->log_output_installed) {
        SDL_SetLogOutputFunction(console->previous_log_output, console->previous_log_userdata);
        console->log_output_installed = false;
    }
    if (console->renderer)
        SDL_DestroyRenderer(console->renderer);
    if (console->window)
        SDL_DestroyWindow(console->window);
    SDL_DestroyMutex(console->log_mutex);
    *console = (DebugConsole){0};
}

void debug_console_draw(DebugConsole *console)
{
    if (!console->renderer)
        return;

    DebugConsoleLine snapshot[DEBUG_CONSOLE_HISTORY_LINES];
    size_t count;
    SDL_LockMutex(console->log_mutex);
    count = console->line_count;
    for (size_t i = 0; i < count; ++i) {
        size_t index = (console->first_line + i) % DEBUG_CONSOLE_HISTORY_LINES;
        snapshot[i] = console->lines[index];
    }
    SDL_UnlockMutex(console->log_mutex);

    SDL_Renderer *renderer = console->renderer;
    SDL_SetRenderDrawColor(renderer, 20, 20, 25, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 230, 230, 230, 255);
    SDL_RenderDebugText(renderer, 10, 10, "littleEngine console initialized.");

    int width, height;
    if (SDL_GetRenderOutputSize(renderer, &width, &height)) {
        (void)width;
        size_t visible = height > 34 ? (size_t)(height - 34) / 12 : 0;
        if (visible > count)
            visible = count;

        for (size_t i = 0; i < visible; ++i) {
            const DebugConsoleLine *line = &snapshot[count - visible + i];
            if (line->priority >= SDL_LOG_PRIORITY_ERROR)
                SDL_SetRenderDrawColor(renderer, 255, 130, 130, 255);
            else if (line->priority == SDL_LOG_PRIORITY_WARN)
                SDL_SetRenderDrawColor(renderer, 255, 220, 120, 255);
            else
                SDL_SetRenderDrawColor(renderer, 230, 230, 230, 255);
            SDL_RenderDebugText(renderer, 10, 28 + (float)i * 12, line->text);
        }
    }
    SDL_RenderPresent(renderer);
}

bool debug_console_event(DebugConsole *console, const SDL_Event *event)
{
    if (!console->window || SDL_GetWindowFromEvent(event) != console->window)
        return false;
    if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
        SDL_HideWindow(console->window);
    return true;
}

void debug_console_wait_for_close(DebugConsole *console)
{
    bool waiting = true;
    SDL_Event event;
    while (waiting) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                 SDL_GetWindowFromEvent(&event) == console->window))
                waiting = false;
        }
        if (!waiting)
            break;
        debug_console_draw(console);
        SDL_Delay(16);
    }
}
