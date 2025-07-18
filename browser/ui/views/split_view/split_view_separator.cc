/* Copyright (c) 2024 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/views/split_view/split_view_separator.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "brave/app/brave_command_ids.h"
#include "brave/browser/ui/color/brave_color_id.h"
#include "brave/components/vector_icons/vector_icons.h"
#include "brave/grit/brave_generated_resources.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "components/grit/brave_components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/text_constants.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/resize_area.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

class MenuButtonDelegate : public views::WidgetDelegateView,
                           public gfx::AnimationDelegate,
                           public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(MenuButtonDelegate, views::WidgetDelegateView)

 public:
  explicit MenuButtonDelegate(Browser* browser) : browser_(browser) {
    BuildMenuModel();
    SetLayoutManager(std::make_unique<views::FillLayout>());
    constexpr auto kCornerRadius = 4;
    constexpr auto kBorderThickness = 1;
    SetBackground(views::CreateRoundedRectBackground(
        kColorBraveSplitViewMenuButtonBackground, kCornerRadius,
        /*for_border_thickness*/ kBorderThickness));
    SetBorder(views::CreateRoundedRectBorder(
        /*thickness*/ kBorderThickness, kCornerRadius,
        kColorBraveSplitViewMenuButtonBorder));

    auto* image_button = AddChildView(views::ImageButton::CreateIconButton(
        base::BindRepeating(&MenuButtonDelegate::OnMenuPressed,
                            base::Unretained(this), browser),
        kLeoMoreVerticalIcon,
        l10n_util::GetStringUTF16(IDS_SPLIT_VIEW_A11Y_SEPARATOR_MENU_BUTTON)));

    auto image_model = ui::ImageModel::FromVectorIcon(
        kLeoMoreVerticalIcon, kColorBraveSplitViewMenuButtonIcon,
        /*icon_size*/ 18);
    for (auto state : views::Button::kButtonStates) {
      image_button->SetImageModel(state, image_model);
    }
    views::InstallRoundRectHighlightPathGenerator(image_button, gfx::Insets(),
                                                  kCornerRadius);

    image_button->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
    image_button->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);

    background_animation_.SetSlideDuration(base::Milliseconds(150));

    SetNotifyEnterExitOnChild(true);
  }

  ~MenuButtonDelegate() override = default;

  // views::WidgetDelegateView:
  void OnMouseEntered(const ui::MouseEvent& event) override {
    background_animation_.Show();
  }

  void OnMouseExited(const ui::MouseEvent& event) override {
    background_animation_.Hide();
  }

  void OnPaintBackground(gfx::Canvas* canvas) override {
    auto scoped_canvas = TransformCanvasForBackground(canvas);
    views::WidgetDelegateView::OnPaintBackground(canvas);
  }

  void OnPaintBorder(gfx::Canvas* canvas) override {
    auto scoped_canvas = TransformCanvasForBackground(canvas);
    views::WidgetDelegateView::OnPaintBorder(canvas);
  }

  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override {
    SchedulePaint();
  }

  void AnimationProgressed(const gfx::Animation* animation) override {
    SchedulePaint();
  }

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override {
    chrome::ExecuteCommand(browser_, command_id);
  }

 private:
  void OnMenuPressed(Browser* browser, const ui::Event& event) {
    CHECK(menu_model_);
    menu_runner_ = std::make_unique<views::MenuRunner>(
        menu_model_.get(), views::MenuRunner::HAS_MNEMONICS);
    menu_runner_->RunMenuAt(GetWidget(), nullptr, GetAnchorBoundsInScreen(),
                            views::MenuAnchorPosition::kTopLeft,
                            ui::mojom::MenuSourceType::kNone);
  }

  void BuildMenuModel() {
    CHECK(!menu_model_);
    menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
    menu_model_->AddItemWithIcon(
        IDC_SWAP_SPLIT_VIEW, l10n_util::GetStringUTF16(IDS_IDC_SWAP_SPLIT_VIEW),
        ui::ImageModel::FromVectorIcon(kLeoSwapHorizontalIcon,
                                       kColorBraveSplitViewMenuItemIcon, 16));
    menu_model_->AddItemWithIcon(
        IDC_BREAK_TILE, l10n_util::GetStringUTF16(IDS_IDC_BREAK_TILE),
        ui::ImageModel::FromVectorIcon(kLeoBrowserSplitViewUnsplitIcon,
                                       kColorBraveSplitViewMenuItemIcon, 16));
  }

  std::unique_ptr<gfx::ScopedCanvas> TransformCanvasForBackground(
      gfx::Canvas* canvas) {
    auto scoped_canvas = std::make_unique<gfx::ScopedCanvas>(canvas);
    gfx::Transform transform;
    transform.Translate(width() / 2, height() / 2);
    transform.Scale(gfx::Tween::DoubleValueBetween(
                        background_animation_.GetCurrentValue(), 0.57, 1),
                    1);
    transform.Translate(-width() / 2, -height() / 2);
    canvas->Transform(transform);
    return scoped_canvas;
  }

  raw_ptr<Browser> browser_ = nullptr;
  gfx::SlideAnimation background_animation_{this};
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
};

BEGIN_METADATA(MenuButtonDelegate)
END_METADATA

SplitViewSeparator::SplitViewSeparator(Browser* browser)
    : ResizeArea(this), browser_(browser) {}

SplitViewSeparator::~SplitViewSeparator() = default;

void SplitViewSeparator::ShowMenuButtonWidget() {
  menu_button_widget_->Show();
}

void SplitViewSeparator::AddedToWidget() {
  ResizeArea::AddedToWidget();

  CreateMenuButton();

  // Invisible by default. Call it after menu button creation to hide it also.
  SetVisible(false);
}

void SplitViewSeparator::VisibilityChanged(views::View* starting_from,
                                           bool is_visible) {
  if (!menu_button_widget_) {
    return;
  }

  if (starting_from != this) {
    return;
  }

  if (is_visible) {
    LayoutMenuButton();
    menu_button_widget_->Show();
  } else {
    menu_button_widget_->Hide();
  }
}

bool SplitViewSeparator::OnMousePressed(const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton() && event.GetClickCount() == 2) {
    if (separator_delegate_) {
      separator_delegate_->OnDoubleClicked();
    }

    return true;
  }

  return ResizeArea::OnMousePressed(event);
}

void SplitViewSeparator::Layout(PassKey) {
  LayoutMenuButton();
}

void SplitViewSeparator::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  ResizeArea::ViewHierarchyChanged(details);

  if (details.is_add && details.child == this) {
    CHECK(!parent_view_observation_.IsObserving())
        << "This is supposed to be called only once.";
    parent_view_observation_.Observe(parent());
  }
}

void SplitViewSeparator::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  ResizeArea::OnBoundsChanged(previous_bounds);

  LayoutMenuButton();
}

void SplitViewSeparator::OnPaintBackground(gfx::Canvas* canvas) {
  auto* cp = GetColorProvider();
  CHECK(cp);

  canvas->FillRect(GetContentsBounds(), cp->GetColor(kColorToolbar));
}

void SplitViewSeparator::OnResize(int resize_amount, bool done_resizing) {
  // When mouse goes toward web contents area, the cursor could have been
  // changed to the normal cursor. Reset it resize cursor.
  GetWidget()->SetCursor(ui::Cursor(ui::mojom::CursorType::kEastWestResize));
  if (resize_area_delegate_) {
    resize_area_delegate_->OnResize(resize_amount, done_resizing);
  }

  if (done_resizing == menu_button_widget_->IsVisible()) {
    return;
  }

  if (done_resizing) {
    LayoutMenuButton();
    menu_button_widget_->Show();
  } else {
    menu_button_widget_->Hide();
  }
}

void SplitViewSeparator::OnWidgetBoundsChanged(views::Widget* widget,
                                               const gfx::Rect& new_bounds) {
  LayoutMenuButton();
}

void SplitViewSeparator::OnViewBoundsChanged(views::View* observed_view) {
  LayoutMenuButton();
}

void SplitViewSeparator::CreateMenuButton() {
  CHECK(!menu_button_widget_);

  menu_button_widget_ = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::Type::TYPE_CONTROL);
  params.delegate = new MenuButtonDelegate(browser_);
  params.parent = GetWidget()->GetNativeView();
  menu_button_widget_->Init(std::move(params));

  parent_widget_observation_.Observe(GetWidget());
}

void SplitViewSeparator::LayoutMenuButton() {
  if (!menu_button_widget_) {
    return;
  }

  constexpr int kMenuButtonSize = 28;
  constexpr int kMenuButtonMarginTop = 8;

  auto menu_button_bounds = ConvertRectToWidget(GetLocalBounds());
  menu_button_bounds.set_x(menu_button_bounds.top_center().x() -
                           kMenuButtonSize / 2);
  menu_button_bounds.set_y(menu_button_bounds.y() + kMenuButtonMarginTop);
  menu_button_bounds.set_size(gfx::Size(kMenuButtonSize, kMenuButtonSize));
  menu_button_widget_->SetBounds(menu_button_bounds);
}

BEGIN_METADATA(SplitViewSeparator)
END_METADATA
