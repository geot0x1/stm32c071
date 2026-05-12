#include "app_state.h"

typedef struct
{
    AppMode mode;
    SystemState state;
} AppState;

static AppState app = {
    .mode = ModeNormal,
    .state = SystemLow,
};

void app_state_init(void)
{
    app.mode = ModeNormal;
    app.state = SystemLow;
}

void app_set_mode(AppMode mode)
{
    app.mode = mode;
}

AppMode app_get_mode(void)
{
    return app.mode;
}

void app_set_state(SystemState state)
{
    app.state = state;
}

SystemState app_get_state(void)
{
    return app.state;
}
