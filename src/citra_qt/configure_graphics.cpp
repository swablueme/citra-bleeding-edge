// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "citra_qt/configure_graphics.h"
#include "core/core.h"
#include "core/settings.h"
#include "ui_configure_graphics.h"

ConfigureGraphics::ConfigureGraphics(QWidget* parent)
    : QWidget(parent), ui(new Ui::ConfigureGraphics) {

    ui->setupUi(this);
    this->setConfiguration();

    ui->toggle_vsync->setEnabled(!Core::System::GetInstance().IsPoweredOn());
}

ConfigureGraphics::~ConfigureGraphics() {}

enum class Resolution : int {
    Auto,
    Scale1x,
    Scale1_5x,
    Scale2x,
    Scale2_5x,
    Scale3x,
    Scale4x,
    Scale5x,
    Scale6x,
    Scale7x,
    Scale8x,
};

float ToResolutionFactor(Resolution option) {
    switch (option) {
    case Resolution::Auto:
        return 0.0;
    case Resolution::Scale1x:
        return 1.0;
    case Resolution::Scale1_5x:
        return 1.5;
    case Resolution::Scale2x:
        return 2.0;
    case Resolution::Scale2_5x:
        return 2.5;
    case Resolution::Scale3x:
        return 3.0;
    case Resolution::Scale4x:
        return 4.0;
    case Resolution::Scale5x:
        return 5.0;
    case Resolution::Scale6x:
        return 6.0;
    case Resolution::Scale7x:
        return 7.0;
    case Resolution::Scale8x:
        return 8.0;
    }
    return 0.0;
}

Resolution FromResolutionFactor(float factor) {
    if (factor == 0.0) {
        return Resolution::Auto;
    } else if (factor == 1.0) {
        return Resolution::Scale1x;
    } else if (factor == 1.5) {
        return Resolution::Scale1_5x;
    } else if (factor == 2.0) {
        return Resolution::Scale2x;
    } else if (factor == 2.5) {
        return Resolution::Scale2_5x;
    } else if (factor == 3.0) {
        return Resolution::Scale3x;
    } else if (factor == 4.0) {
        return Resolution::Scale4x;
    } else if (factor == 5.0) {
        return Resolution::Scale5x;
    } else if (factor == 6.0) {
        return Resolution::Scale6x;
    } else if (factor == 7.0) {
        return Resolution::Scale7x;
    } else if (factor == 8.0) {
        return Resolution::Scale8x;
    }
    return Resolution::Auto;
}

void ConfigureGraphics::setConfiguration() {
    ui->toggle_hw_renderer->setChecked(Settings::values.use_hw_renderer);
    ui->toggle_shader_jit->setChecked(Settings::values.use_shader_jit);
    ui->toggle_scaled_resolution->setChecked(Settings::values.use_scaled_resolution);
    ui->resolution_factor_combobox->setEnabled(Settings::values.use_scaled_resolution);
    ui->resolution_factor_combobox->setCurrentIndex(
        static_cast<int>(FromResolutionFactor(Settings::values.resolution_factor)));
    ui->toggle_vsync->setChecked(Settings::values.use_vsync);
    ui->toggle_framelimit->setChecked(Settings::values.toggle_framelimit);
    ui->layout_combobox->setCurrentIndex(static_cast<int>(Settings::values.layout_option));
    ui->swap_screen->setChecked(Settings::values.swap_screen);
}

void ConfigureGraphics::applyConfiguration() {
    Settings::values.use_hw_renderer = ui->toggle_hw_renderer->isChecked();
    Settings::values.use_shader_jit = ui->toggle_shader_jit->isChecked();
    Settings::values.use_scaled_resolution = ui->toggle_scaled_resolution->isChecked();
    Settings::values.resolution_factor =
        ToResolutionFactor(static_cast<Resolution>(ui->resolution_factor_combobox->currentIndex()));
    Settings::values.use_vsync = ui->toggle_vsync->isChecked();
    Settings::values.toggle_framelimit = ui->toggle_framelimit->isChecked();
    Settings::values.layout_option =
        static_cast<Settings::LayoutOption>(ui->layout_combobox->currentIndex());
    Settings::values.swap_screen = ui->swap_screen->isChecked();
    Settings::Apply();
}
