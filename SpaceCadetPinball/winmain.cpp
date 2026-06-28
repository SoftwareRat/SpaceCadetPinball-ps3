#include "pch.h"
#include "winmain.h"

#include "control.h"
#include "fullscrn.h"
#include "midi.h"
#include "options.h"
#include "pb.h"
#include "render.h"
#include "Sound.h"
#include "translations.h"
#include "font_selection.h"
#include "menu.h"

#include <sys/stat.h>
#include <unistd.h>
#include <sys/tty.h>
#include <string.h>
#include <sys/types.h>
#include <locale>
#include <codecvt>

SDL_Window* winmain::MainWindow = nullptr;
SDL_Renderer* winmain::Renderer = nullptr;
ImGuiIO* winmain::ImIO = nullptr;

int winmain::return_value = 0;
bool winmain::bQuit = false;
bool winmain::activated = false;
int winmain::DispFrameRate = 0;
bool winmain::DispGRhistory = false;
bool winmain::single_step = false;
bool winmain::has_focus = true;
int winmain::last_mouse_x;
int winmain::last_mouse_y;
int winmain::mouse_down;
bool winmain::no_time_loss = false;

bool winmain::restart = false;

gdrv_bitmap8* winmain::gfr_display = nullptr;
bool winmain::ShowAboutDialog = false;
bool winmain::ShowImGuiDemo = false;
bool winmain::ShowSpriteViewer = false;
bool winmain::LaunchBallEnabled = true;
bool winmain::HighScoresEnabled = true;
bool winmain::DemoActive = false;
int winmain::MainMenuHeight = 0;
std::string winmain::FpsDetails, winmain::PrevSdlError;
unsigned winmain::PrevSdlErrorCount = 0;
double winmain::UpdateToFrameRatio;
winmain::DurationMs winmain::TargetFrameTime;
optionsStruct& winmain::Options = options::Options;
winmain::DurationMs winmain::SpinThreshold = DurationMs(0.005);
WelfordState winmain::SleepState{};

bool leftTrigger = false;
bool rightTrigger = false;

int winmain::WinMain(LPCSTR lpCmdLine, char16_t* errorMessage)
{
    {
        const char* msg = "[DBG] winmain::WinMain entered\n";
        sysTtyWrite(0, msg, strlen(msg), NULL);
    }
    
    restart = false;
    bQuit = false;

    std::set_new_handler(memalloc_failure);

    pb::quickFlag = strstr(lpCmdLine, "-quick") != nullptr;

    // SDL window
    {
        const char* msg = "[DBG] SDL_CreateWindow(1920x1080 HIDDEN)...\n";
        sysTtyWrite(0, msg, strlen(msg), NULL);
    }
    SDL_Window* window = SDL_CreateWindow
    (
        pb::get_rc_string(Msg::STRING139),
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        1920, 1080,
        SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE
    );
    MainWindow = window;
    if (!window)
    {
        const char* msg = "[ERROR] SDL_CreateWindow failed\n";
        sysTtyWrite(0, msg, strlen(msg), NULL);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Could not create window", SDL_GetError(), nullptr);
        return 1;
    }
    {
        const char* msg = "[DBG] SDL_CreateWindow OK\n";
        sysTtyWrite(0, msg, strlen(msg), NULL);
    }

    // Hardware Accelerated Renderer (Standard für PS3 RSX via SDL2)
    SDL_Renderer* renderer = nullptr;
    auto swOffset = strstr(lpCmdLine, "-sw") != nullptr ? 1 : 0;
    for (int i = swOffset; i < 2 && !renderer; i++)
    {
        Renderer = renderer = SDL_CreateRenderer
        (
            window,
            -1,
            i == 0 ? SDL_RENDERER_ACCELERATED : SDL_RENDERER_SOFTWARE
        );
    }
    if (!renderer)
    {
        printf("[FATAL] Could not create SDL renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        return 1;
    }
    SDL_RendererInfo rendererInfo{};
    if (!SDL_GetRendererInfo(renderer, &rendererInfo))
        printf("Using SDL renderer: %s\n", rendererInfo.name);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    
    // --- PS3 DEBUG: Quick visual test ---
    SDL_ShowWindow(window);
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
    SDL_Delay(1500);
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
    SDL_Delay(1500);
    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
    SDL_Delay(1500);
    // --- END DEBUG ---

    // ImGui init für PS3 (Reiner SDL2-Renderer-Zweig)
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplSDLRenderer_Init(renderer);
    ImGui_ImplSDLRenderer_NewFrame();
    ImGui::StyleColorsDark();
    ImGuiIO& io = ImGui::GetIO();
    ImIO = &io;
    io.DisplaySize.x = 1920;
    io.DisplaySize.y = 1080;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    io.IniFilename = "/dev_hdd0/game/PINBALL01/USRDIR/imgui_pb.ini";

    // Verzeichnisse auf der PS3-Festplatte anlegen falls nicht vorhanden
    mkdir("/dev_hdd0/game/PINBALL01", 0777);
    mkdir("/dev_hdd0/game/PINBALL01/USRDIR", 0777);

    // Optionen laden
    options::InitPrimary();

    if (!Options.FontFileName.empty())
    {
        ImGui_ImplSDLRenderer_DestroyFontsTexture();
        io.Fonts->Clear();
        ImVector<ImWchar> ranges;
        translations::GetGlyphRange(&ranges);
        ImFontConfig fontConfig{};

        fontConfig.OversampleV = 2;
        fontConfig.OversampleH = 4;

        auto fileName = Options.FontFileName.c_str();
        auto fileHandle = fopenu(fileName, "rb");
        if (fileHandle)
        {
            fclose(fileHandle);
            if (!io.Fonts->AddFontFromFileTTF(fileName, 13.f, &fontConfig, ranges.Data))
                io.Fonts->AddFontDefault();
        }
        else
            io.Fonts->AddFontDefault();

        io.Fonts->Build();
        ImGui_ImplSDLRenderer_CreateFontsTexture();
    }

    // PS3-spezifische Suchpfade für die Spieldatendatei (.DAT)
    std::vector<const char*> searchPaths
    {
        "/dev_hdd0/game/PINBALL01/USRDIR/",
        "/dev_hdd0/game/PINBALL01/USRDIR/data/"
    };
    pb::SelectDatFile(searchPaths);

    options::InitSecondary();

    if (!Sound::Init(Options.SoundChannels, Options.Sounds, Options.SoundVolume))
        Options.Sounds = false;

    if (!pb::quickFlag && !midi::music_init(Options.MusicVolume))
        Options.Music = false;

    if (pb::init())
    {
        // PS3: Show simple SDL color test (no ImGui needed - it may crash)
        SDL_ShowWindow(window);
        
        // Cycle through colors to show SDL rendering works
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
        SDL_Delay(2000);
        
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
        SDL_Delay(2000);
        
        SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
        SDL_Delay(2000);
        
        printf("[ERROR] The .dat file is missing. Put game data in /dev_hdd0/game/PINBALL01/USRDIR/\n");
        
        Sound::Close();
        ImGui_ImplSDLRenderer_Shutdown();
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        ImGui::DestroyContext();
        return 1;
    }

    fullscrn::init();

    pb::reset_table();
    pb::firsttime_setup();

    if (strstr(lpCmdLine, "-fullscreen"))
    {
        Options.FullScreen = true;
    }

    SDL_ShowWindow(window);
    fullscrn::set_screen_mode(Options.FullScreen);

    if (strstr(lpCmdLine, "-demo"))
        pb::toggle_demo();
    else
        pb::replay_level(false);

    unsigned updateCounter = 0, frameCounter = 0;

    auto frameStart = Clock::now();
    double UpdateToFrameCounter = 0;
    DurationMs sleepRemainder(0), frameDuration(TargetFrameTime);
    auto prevTime = frameStart;
    
    while (true)
    {
        if (DispFrameRate)
        {
            auto curTime = Clock::now();
            if (curTime - prevTime > DurationMs(1000))
            {
                char buf[60];
                auto elapsedSec = DurationMs(curTime - prevTime).count() * 0.001;
                snprintf(buf, sizeof buf, "Updates/sec = %02.02f Frames/sec = %02.02f ",
                         updateCounter / elapsedSec, frameCounter / elapsedSec);
                printf("%s\n", buf); // Weiterleitung an PS3 stdout/TTY Logger
                FpsDetails = buf;
                frameCounter = updateCounter = 0;
                prevTime = curTime;
            }
        }

        if (!ProcessWindowMessages() || bQuit)
            break;

        if (has_focus)
        {
            if (pb::cheat_mode && ImIO->MouseDown[ImGuiMouseButton_Left])
            {
                pb::ballset((-ImIO->MouseDelta.x) / (1920 * 2), ImIO->MouseDelta.y / (1080 * 2));
            }
            if (!single_step && !no_time_loss)
            {
                auto dt = static_cast<float>(frameDuration.count());
                pb::frame(dt);
                if (DispGRhistory)
                {
                    auto width = 300;
                    auto height = 64, halfHeight = height / 2;
                    if (!gfr_display)
                    {
                        gfr_display = new gdrv_bitmap8(width, height, false);
                        gfr_display->CreateTexture("nearest", SDL_TEXTUREACCESS_STREAMING);
                    }

                    gdrv::ScrollBitmapHorizontal(gfr_display, -1);
                    gdrv::fill_bitmap(gfr_display, 1, halfHeight, width - 1, 0, ColorRgba::Black());
                    gdrv::fill_bitmap(gfr_display, 1, halfHeight, width - 1, halfHeight, ColorRgba::White());

                    auto target = static_cast<float>(TargetFrameTime.count());
                    auto scale = halfHeight / target;
                    auto diffHeight = std::min(static_cast<int>(round(std::abs(target - dt) * scale)), halfHeight);
                    auto yOffset = dt < target ? halfHeight : halfHeight - diffHeight;
                    gdrv::fill_bitmap(gfr_display, 1, diffHeight, width - 1, yOffset, ColorRgba::Red());
                }
                updateCounter++;
            }
            no_time_loss = false;

            if (UpdateToFrameCounter >= UpdateToFrameRatio)
            {
                // Wii U VPAD/KPAD Polling komplett entfernt.
                // Input wird plattformunabhängig via SDL_Event-Loop verarbeitet.

                ImGui_ImplSDLRenderer_NewFrame();
                ImGui::NewFrame();
                RenderUi();

                SDL_RenderClear(renderer);
                SDL_RenderFillRect(renderer, nullptr);
                render::PresentVScreen();

                ImGui::Render();
                ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());

                SDL_RenderPresent(renderer);
                frameCounter++;
                UpdateToFrameCounter -= UpdateToFrameRatio;
            }

            auto sdlError = SDL_GetError();
            if (sdlError[0] || !PrevSdlError.empty())
            {
                if (sdlError[0])
                    SDL_ClearError();

                if (sdlError != PrevSdlError)
                {
                    PrevSdlError = sdlError;
                    if (PrevSdlErrorCount > 0)
                    {
                        printf("SDL Error: ^ Previous Error Repeated %u Times\n", PrevSdlErrorCount + 1);
                        PrevSdlErrorCount = 0;
                    }

                    if (sdlError[0])
                        printf("SDL Error: %s\n", sdlError);
                }
                else
                {
                    PrevSdlErrorCount++;
                }
            }

            auto updateEnd = Clock::now();
            auto targetTimeDelta = TargetFrameTime - DurationMs(updateEnd - frameStart) - sleepRemainder;

            TimePoint frameEnd;
            if (targetTimeDelta > DurationMs::zero() && !Options.UncappedUpdatesPerSecond)
            {
                if (Options.HybridSleep)
                    HybridSleep(targetTimeDelta);
                else
                    usleep(static_cast<useconds_t>(targetTimeDelta.count() * 1000));
                frameEnd = Clock::now();
            }
            else
            {
                frameEnd = updateEnd;
            }

            sleepRemainder = Clamp(DurationMs(frameEnd - updateEnd) - targetTimeDelta, -TargetFrameTime,
                                   TargetFrameTime);
            frameDuration = std::min<DurationMs>(DurationMs(frameEnd - frameStart), 2 * TargetFrameTime);
            frameStart = frameEnd;
            UpdateToFrameCounter++;
        }
    }

    if (PrevSdlErrorCount > 0)
    {
        printf("SDL Error: ^ Previous Error Repeated %u Times\n", PrevSdlErrorCount);
    }

    delete gfr_display;
    gfr_display = nullptr;
    options::uninit();
    midi::music_shutdown();
    pb::uninit();
    Sound::Close();
    ImGui_ImplSDLRenderer_Shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    ImGui::DestroyContext();

    return return_value;
}

void winmain::RenderUi()
{
    if (!Options.ShowMenu)
    {
        ImGui::SetNextWindowPos(ImVec2{});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{10, 0});
        if (ImGui::Begin("main", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                         ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing))
        {
            ImGui::PushID(1);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{});
            if (ImGui::Button("Menu"))
            {
                options::toggle(Menu1::Show_Menu);
            }
            ImGui::PopStyleColor(1);
            ImGui::PopID();
        }
        ImGui::End();
        ImGui::PopStyleVar();

        if (ImGui::IsNavInputDown(ImGuiNavInput_Cancel))
            ImGui::FocusWindow(nullptr);
    }

#ifndef NDEBUG
    if (ShowImGuiDemo)
        ImGui::ShowDemoWindow(&ShowImGuiDemo);
#endif

    if (menu::ShowWindow)
        menu::RenderMenuWindow();
    a_dialog();
    high_score::RenderHighScoreDialog();
    font_selection::RenderDialog();
    if (ShowSpriteViewer)
        render::SpriteViewer(&ShowSpriteViewer);
    options::RenderControlDialog();
    if (DispGRhistory)
        RenderFrameTimeDialog();

    gdrv::grtext_draw_ttext_in_box();
}

int winmain::event_handler(const SDL_Event* event)
{
    if (ImIO->WantCaptureMouse && !options::WaitingForInput())
    {
        if (mouse_down)
        {
            mouse_down = 0;
            SDL_SetWindowGrab(MainWindow, SDL_FALSE);
        }
        switch (event->type)
        {
        case SDL_MOUSEMOTION:
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEWHEEL:
            return 1;
        default: ;
        }
    }
    if (ImIO->WantCaptureKeyboard && !options::WaitingForInput())
    {
        switch (event->type)
        {
        case SDL_KEYDOWN:
        case SDL_KEYUP:
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
            return 1;
        default: ;
        }
    }

    switch (event->type)
    {
    case SDL_QUIT:
        end_pause();
        bQuit = true;
        fullscrn::shutdown();
        return_value = 0;
        return 0;
    case SDL_KEYUP:
        pb::InputUp({InputTypes::Keyboard, (unsigned int)event->key.keysym.sym});
        break;
    case SDL_KEYDOWN:
        if (!event->key.repeat)
            pb::InputDown({InputTypes::Keyboard, (unsigned int)event->key.keysym.sym});
        switch (event->key.keysym.sym)
        {
        case SDLK_ESCAPE:
            if (Options.FullScreen)
                options::toggle(Menu1::Full_Screen);
            SDL_MinimizeWindow(MainWindow);
            break;
        case SDLK_F2:
            new_game();
            break;
        case SDLK_F3:
            pause();
            break;
        case SDLK_F4:
            options::toggle(Menu1::Full_Screen);
            break;
        case SDLK_F5:
            options::toggle(Menu1::Sounds);
            break;
        case SDLK_F6:
            options::toggle(Menu1::Music);
            break;
        case SDLK_F8:
            pause(false);
            options::ShowControlDialog();
            break;
        case SDLK_F9:
            options::toggle(Menu1::Show_Menu);
            break;
        default:
            break;
        }

        if (!pb::cheat_mode)
            break;

        switch (event->key.keysym.sym)
        {
        case SDLK_g:
            DispGRhistory ^= true;
            break;
        case SDLK_o:
            {
                auto plt = new ColorRgba[4 * 256];
                auto pltPtr = &plt[10];
                for (int i1 = 0, i2 = 0; i1 < 256 - 10; ++i1, i2 += 8)
                {
                    unsigned char blue = i2, redGreen = i2;
                    if (i2 > 255)
                    {
                        blue = 255;
                        redGreen = i1;
                    }

                    *pltPtr++ = ColorRgba{blue, redGreen, redGreen, 0};
                }
                gdrv::display_palette(plt);
                delete[] plt;
            }
            break;
        case SDLK_y:
            SDL_SetWindowTitle(MainWindow, "Pinball");
            DispFrameRate = DispFrameRate == 0;
            break;
        case SDLK_F1:
            pb::frame(10);
            break;
        case SDLK_F10:
            single_step ^= true;
            if (!single_step)
                no_time_loss = true;
            break;
        default:
            break;
        }
        break;
        
    // PS3 Gamepad Input Handling via SDL2 Events
    case SDL_CONTROLLERBUTTONDOWN:
        if (!ImIO->WantCaptureKeyboard)
            pb::InputDown({InputTypes::Keyboard, event->cbutton.button});
        break;
    case SDL_CONTROLLERBUTTONUP:
        if (!ImIO->WantCaptureKeyboard)
            pb::InputUp({InputTypes::Keyboard, event->cbutton.button});
        break;

    case SDL_MOUSEBUTTONDOWN:
        {
            bool noInput = false;
            switch (event->button.button)
            {
            case SDL_BUTTON_LEFT:
                if (pb::cheat_mode)
                {
                    mouse_down = 1;
                    last_mouse_x = event->button.x;
                    last_mouse_y = event->button.y;
                    SDL_SetWindowGrab(MainWindow, SDL_TRUE);
                    noInput = true;
                }
                break;
            default:
                break;
            }

            if (!noInput)
                pb::InputDown({InputTypes::Mouse, event->button.button});
        }
        break;
    case SDL_MOUSEBUTTONUP:
        {
            bool noInput = false;
            switch (event->button.button)
            {
            case SDL_BUTTON_LEFT:
                if (mouse_down)
                {
                    mouse_down = 0;
                    SDL_SetWindowGrab(MainWindow, SDL_FALSE);
                    noInput = true;
                }
                break;
            default:
                break;
            }

            if (!noInput)
                pb::InputUp({InputTypes::Mouse, event->button.button});
        }
        break;
    case SDL_WINDOWEVENT:
        switch (event->window.event)
        {
        case SDL_WINDOWEVENT_FOCUS_GAINED:
        case SDL_WINDOWEVENT_TAKE_FOCUS:
        case SDL_WINDOWEVENT_SHOWN:
            activated = true;
            Sound::Activate();
            if (Options.Music && !single_step)
                midi::music_play();
            no_time_loss = true;
            has_focus = true;
            break;
        case SDL_WINDOWEVENT_FOCUS_LOST:
        case SDL_WINDOWEVENT_HIDDEN:
            activated = false;
            fullscrn::activate(0);
            Options.FullScreen = false;
            Sound::Deactivate();
            midi::music_stop();
            has_focus = false;
            pb::loose_focus();
            break;
        case SDL_WINDOWEVENT_SIZE_CHANGED:
        case SDL_WINDOWEVENT_RESIZED:
            fullscrn::window_size_changed();
            break;
        default: ;
        }
        break;
    default: ;
    }

    return 1;
}

int winmain::ProcessWindowMessages()
{
    static auto idleWait = 0;
    SDL_Event event;
    if (has_focus)
    {
        idleWait = static_cast<int>(TargetFrameTime.count());
        while (SDL_PollEvent(&event))
        {
            if (!event_handler(&event))
                return 0;
        }

        return 1;
    }

    idleWait = std::min(idleWait + static_cast<int>(TargetFrameTime.count()), 500);
    if (SDL_WaitEventTimeout(&event, idleWait))
    {
        idleWait = static_cast<int>(TargetFrameTime.count());
        return event_handler(&event);
    }
    return 1;
}

void winmain::memalloc_failure()
{
    midi::music_stop();
    Sound::Close();
    const char* caption = pb::get_rc_string(Msg::STRING270);
    const char* text = pb::get_rc_string(Msg::STRING279);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, caption, text, MainWindow);
    std::exit(1);
}

void winmain::a_dialog()
{
    if (ShowAboutDialog == true)
    {
        ShowAboutDialog = false;
        ImGui::OpenPopup(pb::get_rc_string(Msg::STRING204));
    }

    bool unused_open = true;
    if (ImGui::BeginPopupModal(pb::get_rc_string(Msg::STRING204), &unused_open, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted(pb::get_rc_string(Msg::STRING139));
        ImGui::TextUnformatted("Original game by Cinematronics, Microsoft");
        ImGui::Separator();

        ImGui::TextUnformatted("Decompiled -> Ported to SDL (PS3-Port via PSL1GHT)");
        ImGui::TextUnformatted("Version 2.0.1");
        ImGui::Separator();

        if (ImGui::Button("Ok"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void winmain::end_pause()
{
    if (single_step)
    {
        pb::pause_continue();
        no_time_loss = true;
    }
}

void winmain::new_game()
{
    end_pause();
    pb::replay_level(false);
}

void winmain::pause(bool toggle)
{
    if (toggle || !single_step)
    {
        pb::pause_continue();
        no_time_loss = true;
    }
}

void winmain::Restart()
{
    restart = true;
    SDL_Event event{SDL_QUIT};
    SDL_PushEvent(&event);
}

void winmain::UpdateFrameRate()
{
    auto fps = Options.FramesPerSecond, ups = Options.UpdatesPerSecond;
    UpdateToFrameRatio = static_cast<double>(ups) / fps;
    TargetFrameTime = DurationMs(1000.0 / ups);
}

void winmain::RenderFrameTimeDialog()
{
    if (!gfr_display)
        return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2{300, 70});
    if (ImGui::Begin("Frame Times", &DispGRhistory, ImGuiWindowFlags_NoScrollbar))
    {
        auto target = static_cast<float>(TargetFrameTime.count());
        auto scale = 1 / (gfr_display->Height / 2 / target);

        auto spin = Options.HybridSleep ? static_cast<float>(SpinThreshold.count()) : 0;
        ImGui::Text("Target frame time:%03.04fms, 1px:%03.04fms, SpinThreshold:%03.04fms",
                    target, scale, spin);
        gfr_display->BlitToTexture();
        auto region = ImGui::GetContentRegionAvail();
        ImGui::Image(gfr_display->Texture, region);
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void winmain::HybridSleep(DurationMs sleepTarget)
{
    static constexpr double StdDevFactor = 0.5;

    while (sleepTarget > SpinThreshold)
    {
        auto start = Clock::now();
        usleep(1000); // 1ms sleep
        auto end = Clock::now();

        auto actualDuration = DurationMs(end - start);
        sleepTarget -= actualDuration;

        SleepState.Advance(actualDuration.count());
        SpinThreshold = DurationMs(SleepState.mean + SleepState.GetStdDev() * StdDevFactor);
    }

    for (auto start = Clock::now(); DurationMs(Clock::now() - start) < sleepTarget;);
}