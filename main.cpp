#include <conio.h>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>

import dcp.nvidia;

struct MenuSettings
{
    int brightness = 50;
    int contrast = 50;
    double gamma = 1.0;
    int vibrance = 50;
    int hue = 0;
};

void clearScreen()
{
    system("cls");
}

void drawBar(
    const std::string& name,
    double value,
    double min,
    double max,
    bool selected,
    int width = 30)
{
    const double normalized =
        (value - min) /
        (max - min);

    const int filled =
        static_cast<int>(
            normalized * width);

    std::cout
        << (selected ? "> " : "  ")
        << std::left
        << std::setw(18)
        << name
        << "[";

    for (int i = 0; i < width; i++) {
        std::cout << (i < filled ? '#' : '-');
    }

    std::cout
        << "] "
        << std::fixed
        << std::setprecision(2)
        << value
        << '\n';
}

void drawMenu(
    const MenuSettings& settings,
    int selected)
{
    clearScreen();

    std::cout
        << "NVIDIA Desktop Color Settings\n\n";

    drawBar(
        "Brightness",
        settings.brightness,
        0,
        100,
        selected == 0);

    drawBar(
        "Contrast",
        settings.contrast,
        0,
        100,
        selected == 1);

    drawBar(
        "Gamma",
        settings.gamma,
        0.4,
        2.8,
        selected == 2);

    drawBar(
        "Digital Vibrance",
        settings.vibrance,
        0,
        100,
        selected == 3);

    drawBar(
        "Hue",
        settings.hue,
        0,
        359,
        selected == 4);

    std::cout
        << "\n"
        << "UP/DOWN    Select\n"
        << "LEFT/RIGHT Change value\n"
        << "R           Reset\n"
        << "ESC         Exit\n";
}

int main()
{
    dcp::nvidia::DCP dcp;

    MenuSettings settings;

    int selected = 0;

    constexpr int MENU_ITEMS = 5;

    bool running = true;

    while (running) {
        drawMenu(
            settings,
            selected);

        const int key =
            _getch();

        // Arrow keys on Windows return
        // 0 or 224 first, followed by the real key code.
        if (key == 0 || key == 224) {
            const int arrow =
                _getch();

            switch (arrow) {
                // UP
                case 72:
                    selected--;

                    if (selected < 0)
                        selected = MENU_ITEMS - 1;

                    break;

                // DOWN
                case 80:
                    selected++;

                    if (selected >= MENU_ITEMS)
                        selected = 0;

                    break;

                // LEFT
                case 75:
                    switch (selected) {
                        case 0:
                            settings.brightness =
                                std::max(
                                    0,
                                    settings.brightness - 1);

                            dcp.setBrightness(
                                settings.brightness);
                            break;

                        case 1:
                            settings.contrast =
                                std::max(
                                    0,
                                    settings.contrast - 1);

                            dcp.setContrast(
                                settings.contrast);
                            break;

                        case 2:
                            settings.gamma =
                                std::max(
                                    0.4,
                                    settings.gamma - 0.05);

                            dcp.setGamma(
                                settings.gamma);
                            break;

                        case 3:
                            settings.vibrance =
                                std::max(
                                    0,
                                    settings.vibrance - 1);

                            dcp.setDigitalVibrance(
                                settings.vibrance);
                            break;

                        case 4:
                            settings.hue--;

                            if (settings.hue < 0)
                                settings.hue = 359;

                            dcp.setHue(
                                settings.hue);
                            break;
                    }

                    break;

                // RIGHT
                case 77:
                    switch (selected) {
                        case 0:
                            settings.brightness =
                                std::min(
                                    100,
                                    settings.brightness + 1);

                            dcp.setBrightness(
                                settings.brightness);
                            break;

                        case 1:
                            settings.contrast =
                                std::min(
                                    100,
                                    settings.contrast + 1);

                            dcp.setContrast(
                                settings.contrast);
                            break;

                        case 2:
                            settings.gamma =
                                std::min(
                                    2.8,
                                    settings.gamma + 0.05);

                            dcp.setGamma(
                                settings.gamma);
                            break;

                        case 3:
                            settings.vibrance =
                                std::min(
                                    100,
                                    settings.vibrance + 1);

                            dcp.setDigitalVibrance(
                                settings.vibrance);
                            break;

                        case 4:
                            settings.hue++;

                            if (settings.hue > 359)
                                settings.hue = 0;

                            dcp.setHue(
                                settings.hue);
                            break;
                    }

                    break;
            }

            continue;
        }


        // ESC
        if (key == 27) {
            running = false;
            continue;
        }


        // R / r
        if (key == 'r' || key == 'R') {
            settings =
            {
                50,
                50,
                1.0,
                50,
                0
            };

            dcp.setBrightness(
                settings.brightness);

            dcp.setContrast(
                settings.contrast);

            dcp.setGamma(
                settings.gamma);

            dcp.setDigitalVibrance(
                settings.vibrance);

            dcp.setHue(
                settings.hue);
        }
    }

    return 0;
}