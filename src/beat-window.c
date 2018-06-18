/* beat-window.c
 *
 * Copyright 2018 Matthias Vogelgesang
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <canberra.h>
#include "beat-config.h"
#include "beat-window.h"

struct _BeatWindow
{
  GtkApplicationWindow  parent_instance;

  GtkHeaderBar  *header_bar;
  GtkMenuButton *menu_button;
  GtkSpinButton *bpm_entry;
  GtkScale      *bpm_scale;
  GtkButton     *play_button;
  GtkWidget     *play_image;
  GtkWidget     *stop_image;

  ca_context    *canberra;
  gboolean       playing;
  guint          beep_source;
  guint          interval;
};

G_DEFINE_TYPE (BeatWindow, beat_window, GTK_TYPE_APPLICATION_WINDOW)

typedef struct {
  BeatWindow    *self;
  guint          source;
} BeepData;

static gboolean
beep (gpointer user_data)
{
  BeepData *data;
  gboolean cont;

  data = (BeepData *) user_data;
  cont = data->source == data->self->beep_source && data->self->playing;

  if (cont) {
    gint error;

    error = ca_context_play (data->self->canberra, 0, CA_PROP_EVENT_ID, "message", NULL);

    if (error < 0)
      g_printerr ("ca %i: %s\n", error, ca_strerror (error));
  }

  return cont;
}

static void
setup_timer (BeatWindow *self)
{
  gdouble bpm;
  BeepData *data;

  bpm = (guint) gtk_range_get_value (GTK_RANGE (self->bpm_scale));
  self->interval = (guint) (1000.0 / (bpm / 60.0));

  data = g_new0 (BeepData, 1);
  data->self = self;
  data->source = self->beep_source = g_timeout_add_full (G_PRIORITY_HIGH, self->interval, beep, data, g_free);
}

static void
on_bpm_changed (BeatWindow *self,
                GtkRange *range)
{
  if (self->playing)
    setup_timer (self);
}

static void
play_toggled (GSimpleAction *action,
              GVariant *param,
              gpointer user_data)
{
  BeatWindow *self;
  const gchar *icon_name;
  GtkWidget *image;
  GVariant *state;

  self = BEAT_WINDOW (user_data);
  state = g_action_get_state (G_ACTION (action));
  self->playing = !g_variant_get_boolean (state);
  g_simple_action_set_state (action, g_variant_new_boolean (self->playing));
  g_variant_unref (state);

  icon_name = self->playing ? "media-playback-stop-symbolic" : "media-playback-start-symbolic";
  image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image (self->play_button, image);

  if (self->playing)
    setup_timer (self);
  else
    self->beep_source = 0;
}

static void
show_about (GSimpleAction *action,
            GVariant *param,
            gpointer user_data)
{
  static const gchar *authors[] = {
    "Matthias Vogelgesang <matthias.vogelgesang@gmail.com>",
    NULL
  };

  gtk_show_about_dialog (GTK_WINDOW (user_data),
                         "program-name", "Beat",
                         "copyright", "Copyright \xC2\xA9 The Beat authors",
                         "license-type", GTK_LICENSE_GPL_3_0,
                         "authors", authors,
                         NULL);
}

static void
beat_window_constructed (GObject *object)
{
  BeatWindow *self;
  GMenuModel *menu;
  g_autoptr (GtkStyleProvider) provider;
  g_autoptr (GtkBuilder) builder;

  static GActionEntry entries[] = {
    { "play",   NULL,       NULL, "false",    play_toggled },
    { "about",  show_about, NULL, NULL,       NULL },
  };

  self = BEAT_WINDOW (object);

  /* actions */
  g_action_map_add_action_entries (G_ACTION_MAP (self), entries, G_N_ELEMENTS (entries), self);

  /* slider and spin button */
  gtk_range_set_range (GTK_RANGE (self->bpm_scale), 1, 240);
  gtk_range_set_value (GTK_RANGE (self->bpm_scale), 120);
  gtk_spin_button_set_adjustment (self->bpm_entry, gtk_range_get_adjustment (GTK_RANGE (self->bpm_scale)));
  gtk_spin_button_set_increments (self->bpm_entry, 1, 5);

  /* images */
  gtk_button_set_image (self->play_button, self->play_image);

  /* css */
  provider = GTK_STYLE_PROVIDER (gtk_css_provider_get_default ());
  gtk_css_provider_load_from_resource (GTK_CSS_PROVIDER (provider), "/net/bloerg/Beat/css/beat.css");
  gtk_style_context_add_provider_for_screen (gtk_window_get_screen (GTK_WINDOW (self)), 
                                             provider, GTK_STYLE_PROVIDER_PRIORITY_USER);

  /* menu */
  builder = gtk_builder_new_from_resource ("/net/bloerg/Beat/ui/beat-menu.ui");
  menu = G_MENU_MODEL (gtk_builder_get_object (builder, "menu"));
  gtk_menu_button_set_menu_model (self->menu_button, menu);

  /* canberra */
  ca_context_create (&self->canberra);

  G_OBJECT_CLASS (beat_window_parent_class)->constructed (object);
}

static void
beat_window_finalize (GObject *object)
{
  BeatWindow *self;

  self = BEAT_WINDOW (object);
  ca_context_destroy (self->canberra);

  G_OBJECT_CLASS (beat_window_parent_class)->finalize (object);
}

static void
beat_window_class_init (BeatWindowClass *klass)
{
  GObjectClass *oclass;
  GtkWidgetClass *widget_class;

  oclass = G_OBJECT_CLASS (klass);
  widget_class = GTK_WIDGET_CLASS (klass);

  oclass->constructed = beat_window_constructed;
  oclass->finalize = beat_window_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/net/bloerg/Beat/ui/beat-window.ui");
  gtk_widget_class_bind_template_child (widget_class, BeatWindow, header_bar);
  gtk_widget_class_bind_template_child (widget_class, BeatWindow, menu_button);
  gtk_widget_class_bind_template_child (widget_class, BeatWindow, bpm_entry);
  gtk_widget_class_bind_template_child (widget_class, BeatWindow, bpm_scale);
  gtk_widget_class_bind_template_child (widget_class, BeatWindow, play_button);

  gtk_widget_class_bind_template_callback (widget_class, on_bpm_changed);
}

static void
beat_window_init (BeatWindow *self)
{
  self->playing = FALSE;
  self->beep_source = 0;
  gtk_widget_init_template (GTK_WIDGET (self));
}
