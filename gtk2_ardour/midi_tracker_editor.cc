/*
    Copyright (C) 2015 Nil Geisweiller

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <cmath>
#include <map>

#include <gtkmm/cellrenderercombo.h>

#include "pbd/file_utils.h"

#include "evoral/midi_util.h"
#include "evoral/Note.hpp"

#include "ardour/beats_frames_converter.h"
#include "ardour/midi_model.h"
#include "ardour/midi_region.h"
#include "ardour/midi_source.h"
#include "ardour/session.h"
#include "ardour/tempo.h"

#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/rgb_macros.h"

#include "ui_config.h"
#include "midi_tracker_editor.h"
#include "note_player.h"
#include "tooltips.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace Glib;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Editing;
using Timecode::BBT_Time;

///////////////////////
// MidiTrackerEditor //
///////////////////////

static const gchar *_beats_per_row_strings[] = {
	N_("Beats/128"),
	N_("Beats/64"),
	N_("Beats/32"),
	N_("Beats/28"),
	N_("Beats/24"),
	N_("Beats/20"),
	N_("Beats/16"),
	N_("Beats/14"),
	N_("Beats/12"),
	N_("Beats/10"),
	N_("Beats/8"),
	N_("Beats/7"),
	N_("Beats/6"),
	N_("Beats/5"),
	N_("Beats/4"),
	N_("Beats/3"),
	N_("Beats/2"),
	N_("Beats"),
	0
};

#define COMBO_TRIANGLE_WIDTH 25

const std::string MidiTrackerEditor::note_off_str = "===";
const std::string MidiTrackerEditor::undefined_str = "***";

MidiTrackerEditor::MidiTrackerEditor (ARDOUR::Session* s, boost::shared_ptr<ARDOUR::Route> rou, boost::shared_ptr<MidiRegion> reg, boost::shared_ptr<MidiTrack> tr)
	: ArdourWindow (reg->name())
	, route(rou)
	, myactions (X_("Tracking"))
	, visible_blank (false)
	, visible_note (true)
	, visible_channel (false)
	, visible_velocity (true)
	, visible_delay (true)
	, automation_action_menu(0)
	, region (reg)
	, track (tr)
	, midi_model (region->midi_source(0)->model())

{
	/* We do not handle nested sources/regions. Caller should have tackled this */

	if (reg->max_source_level() > 0) {
		throw failed_constructor();
	}

	set_session (s);

	// Beats per row combo
	beats_per_row_strings =  I18N (_beats_per_row_strings);
	build_beats_per_row_menu ();

	register_actions ();

	setup_tooltips ();
	setup_toolbar ();
	setup_matrix ();
	setup_scroller ();

	setup_processor_menu_and_curves ();

	set_beats_per_row_to (SnapToBeatDiv4);

	redisplay_model ();

	midi_model->ContentsChanged.connect (content_connection, invalidator (*this),
	                                     boost::bind (&MidiTrackerEditor::redisplay_model, this), gui_context());

	vbox.show ();

	vbox.set_spacing (6);
	vbox.set_border_width (6);
	vbox.pack_start (toolbar, false, false);
	vbox.pack_start (scroller, true, true);

	add (vbox);
	set_size_request (-1, 400);
}

MidiTrackerEditor::~MidiTrackerEditor ()
{
	delete mtm;
}

void
MidiTrackerEditor::register_actions ()
{
	Glib::RefPtr<ActionGroup> beats_per_row_actions = myactions.create_action_group (X_("BeatsPerRow"));
	RadioAction::Group beats_per_row_choice_group;

	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-onetwentyeighths"), _("Beats Per Row to One Twenty Eighths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv128)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-sixtyfourths"), _("Beats Per Row to Sixty Fourths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv64)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-thirtyseconds"), _("Beats Per Row to Thirty Seconds"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv32)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-twentyeighths"), _("Beats Per Row to Twenty Eighths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv28)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-twentyfourths"), _("Beats Per Row to Twenty Fourths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv24)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-twentieths"), _("Beats Per Row to Twentieths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv20)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-asixteenthbeat"), _("Beats Per Row to Sixteenths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv16)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-fourteenths"), _("Beats Per Row to Fourteenths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv14)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-twelfths"), _("Beats Per Row to Twelfths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv12)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-tenths"), _("Beats Per Row to Tenths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv10)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-eighths"), _("Beats Per Row to Eighths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv8)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-sevenths"), _("Beats Per Row to Sevenths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv7)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-sixths"), _("Beats Per Row to Sixths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv6)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-fifths"), _("Beats Per Row to Fifths"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv5)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-quarters"), _("Beats Per Row to Quarters"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv4)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-thirds"), _("Beats Per Row to Thirds"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv3)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-halves"), _("Beats Per Row to Halves"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeatDiv2)));
	myactions.register_radio_action (beats_per_row_actions, beats_per_row_choice_group, X_("beats-per-row-beat"), _("Beats Per Row to Beat"), (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_chosen), Editing::SnapToBeat)));
}

bool
MidiTrackerEditor::visible_blank_press(GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	visible_blank = !visible_blank;
	if (visible_blank)
		visible_blank_button.set_active_state (Gtkmm2ext::ExplicitActive);
	else
		visible_blank_button.set_active_state (Gtkmm2ext::Off);
	redisplay_model ();
	return false;
}

void
MidiTrackerEditor::redisplay_visible_note()
{
	for (size_t i = 0; i < MAX_NUMBER_OF_TRACKS; i++)
		view.get_column(i*4 + 1)->set_visible(i < mtm->ntracks ? visible_note : false);
	visible_note_button.set_active_state (visible_note ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
}

bool
MidiTrackerEditor::visible_note_press(GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	visible_note = !visible_note;
	redisplay_visible_note();
	return false;
}

void
MidiTrackerEditor::redisplay_visible_channel()
{
	for (size_t i = 0; i < MAX_NUMBER_OF_TRACKS; i++)
		view.get_column(i*4 + 2)->set_visible(i < mtm->ntracks ? visible_channel : false);
	visible_channel_button.set_active_state (visible_channel ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
}

bool
MidiTrackerEditor::visible_channel_press(GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	visible_channel = !visible_channel;
	redisplay_visible_channel();
	return false;
}

void
MidiTrackerEditor::redisplay_visible_velocity()
{
	for (size_t i = 0; i < MAX_NUMBER_OF_TRACKS; i++)
		view.get_column(i*4 + 3)->set_visible(i < mtm->ntracks ? visible_velocity : false);
	visible_velocity_button.set_active_state (visible_velocity ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
}

bool
MidiTrackerEditor::visible_velocity_press(GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	visible_velocity = !visible_velocity;
	redisplay_visible_velocity();
	return false;
}

void
MidiTrackerEditor::redisplay_visible_delay()
{
	for (size_t i = 0; i < MAX_NUMBER_OF_TRACKS; i++)
		view.get_column(i*4 + 4)->set_visible(i < mtm->ntracks ? visible_delay : false);
	visible_delay_button.set_active_state (visible_delay ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
}

bool
MidiTrackerEditor::visible_delay_press(GdkEventButton* ev)
{
	/* ignore double/triple clicks */
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	visible_delay = !visible_delay;
	redisplay_visible_delay();
	return false;
}

void
MidiTrackerEditor::automation_click ()
{
	build_automation_action_menu ();
	automation_action_menu->popup (1, gtk_get_current_event_time());
}

void
MidiTrackerEditor::redisplay_model ()
{
	view.set_model (Glib::RefPtr<Gtk::ListStore>(0));
	model->clear ();

	if (_session) {

		mtm->set_rows_per_beat(rows_per_beat);
		mtm->update_matrix();

		TreeModel::Row row;
		
		// Generate each row
		for (uint32_t irow = 0; irow < mtm->nrows; irow++) {
			row = *(model->append());
			Evoral::Beats row_beats = mtm->beats_at_row(irow);
			uint32_t row_frame = mtm->frame_at_row(irow);

			// Time
			Timecode::BBT_Time row_bbt;
			_session->tempo_map().bbt_time(row_frame, row_bbt);
			stringstream ss;
			print_padded(ss, row_bbt);
			row[columns.time] = ss.str();

			// If the row is on a beat the color differs
			row[columns._color] = row_beats == row_beats.round_up_to_beat() ?
				"#202020" : "#101010";

			// TODO: don't dismiss off-beat rows near the region boundaries

			for (size_t i = 0; i < (size_t)mtm->ntracks; i++) {

				if (visible_blank) {
					// Fill with blank
					row[columns.note_name[i]] = "----";
					row[columns.channel[i]] = "--";
					row[columns.velocity[i]] = "---";
					row[columns.delay[i]] = "-----";
				}
				
				size_t notes_off_count = mtm->notes_off[i].count(irow);
				size_t notes_on_count = mtm->notes_on[i].count(irow);

				if (notes_on_count > 0 || notes_off_count > 0) {
					MidiTrackerMatrix::RowToNotes::const_iterator i_off = mtm->notes_off[i].find(irow);
					MidiTrackerMatrix::RowToNotes::const_iterator i_on = mtm->notes_on[i].find(irow);

					// Determine whether the row is defined
					bool undefined = (notes_off_count > 1 || notes_on_count > 1)
						|| (notes_off_count == 1 && notes_on_count == 1
						    && i_off->second->end_time() != i_on->second->time());

					if (undefined) {
						row[columns.note_name[i]] = undefined_str;
					} else {
						// Notes off
						MidiTrackerMatrix::RowToNotes::const_iterator i_off = mtm->notes_off[i].find(irow);
						if (i_off != mtm->notes_off[i].end()) {
							boost::shared_ptr<NoteType> note = i_off->second;
							row[columns.note_name[i]] = note_off_str;
							row[columns.channel[i]] = to_string (note->channel() + 1);
							row[columns.velocity[i]] = to_string ((int)note->velocity());
							int64_t delay_ticks = (note->end_time() - row_beats).to_relative_ticks();
							if (delay_ticks != 0)
								row[columns.delay[i]] = to_string(delay_ticks);
						}

						// Notes on
						MidiTrackerMatrix::RowToNotes::const_iterator i_on = mtm->notes_on[i].find(irow);
						if (i_on != mtm->notes_on[i].end()) {
							boost::shared_ptr<NoteType> note = i_on->second;
							row[columns.channel[i]] = to_string (note->channel() + 1);
							row[columns.note_name[i]] = Evoral::midi_note_name (note->note());
							row[columns.velocity[i]] = to_string ((int)note->velocity());

							int64_t delay_ticks = (note->time() - row_beats).to_relative_ticks();
							if (delay_ticks != 0) {
								row[columns.delay[i]] = to_string (delay_ticks);
							}
							// Keep the note around for playing it
							row[columns._note[i]] = note;
						}
					}
				}
			}
		}
	}
	view.set_model (model);

	// In case tracks have been added or removed
	redisplay_visible_note();
	redisplay_visible_channel();
	redisplay_visible_velocity();
	redisplay_visible_delay();
}

void
MidiTrackerEditor::setup_matrix ()
{
	mtm = new MidiTrackerMatrix(_session, region, midi_model, rows_per_beat);

	edit_column = -1;
	editing_renderer = 0;
	editing_editable = 0;

	model = ListStore::create (columns);
	view.set_model (model);

	Gtk::TreeViewColumn* viewcolumn_time  = new Gtk::TreeViewColumn (_("Time"), columns.time);
	Gtk::CellRenderer* cellrenderer_time = viewcolumn_time->get_first_cell_renderer ();		
	viewcolumn_time->add_attribute(cellrenderer_time->property_cell_background (), columns._color);
	view.append_column (*viewcolumn_time);
	for (size_t i = 0; i < MAX_NUMBER_OF_TRACKS; i++) {
		stringstream ss_note;
		stringstream ss_ch;
		stringstream ss_vel;
		stringstream ss_delay;
		ss_note << "Note"; // << i;
		ss_ch << "Ch"; // << i;
		ss_vel << "Vel"; // << i;
		ss_delay << "Delay"; // << i;

		// TODO be careful of potential memory leaks
		Gtk::TreeViewColumn* viewcolumn_note = new Gtk::TreeViewColumn (_(ss_note.str().c_str()), columns.note_name[i]);
		Gtk::TreeViewColumn* viewcolumn_channel = new Gtk::TreeViewColumn (_(ss_ch.str().c_str()), columns.channel[i]);
		Gtk::TreeViewColumn* viewcolumn_velocity = new Gtk::TreeViewColumn (_(ss_vel.str().c_str()), columns.velocity[i]);
		Gtk::TreeViewColumn* viewcolumn_delay = new Gtk::TreeViewColumn (_(ss_delay.str().c_str()), columns.delay[i]);

		Gtk::CellRenderer* cellrenderer_note = viewcolumn_note->get_first_cell_renderer ();		
		Gtk::CellRenderer* cellrenderer_channel = viewcolumn_channel->get_first_cell_renderer ();		
		Gtk::CellRenderer* cellrenderer_velocity = viewcolumn_velocity->get_first_cell_renderer ();		
		Gtk::CellRenderer* cellrenderer_delay = viewcolumn_delay->get_first_cell_renderer ();		

		viewcolumn_note->add_attribute(cellrenderer_note->property_cell_background (), columns._color);
		viewcolumn_channel->add_attribute(cellrenderer_channel->property_cell_background (), columns._color);
		viewcolumn_velocity->add_attribute(cellrenderer_velocity->property_cell_background (), columns._color);
		viewcolumn_delay->add_attribute(cellrenderer_delay->property_cell_background (), columns._color);

		view.append_column (*viewcolumn_note);
		view.append_column (*viewcolumn_channel);
		view.append_column (*viewcolumn_velocity);
		view.append_column (*viewcolumn_delay);
	}

	view.set_headers_visible (true);
	view.set_rules_hint (true);
	view.set_grid_lines (TREE_VIEW_GRID_LINES_BOTH);
	view.get_selection()->set_mode (SELECTION_MULTIPLE);

	view.show ();
}

void
MidiTrackerEditor::setup_toolbar ()
{
	uint32_t inactive_button_color = RGBA_TO_UINT(255, 255, 255, 64);
	uint32_t active_button_color = RGBA_TO_UINT(168, 248, 48, 255);
	toolbar.set_spacing (2);

	// Add beats per row selection
	beats_per_row_selector.show ();
	toolbar.pack_start (beats_per_row_selector, false, false);

	// Add visible blank button
	visible_blank_button.set_name ("visible blank button");
	visible_blank_button.set_text (S_("---|-"));
	visible_blank_button.set_fixed_colors(active_button_color, inactive_button_color);
	visible_blank_button.set_active_state (visible_blank ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	visible_blank_button.show ();
	visible_blank_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MidiTrackerEditor::visible_blank_press), false);
	toolbar.pack_start (visible_blank_button, false, false);

	// Add visible note button
	visible_note_button.set_name ("visible note button");
	visible_note_button.set_text (S_("Note|N"));
	visible_note_button.set_fixed_colors(active_button_color, inactive_button_color);
	visible_note_button.set_active_state (visible_note ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	visible_note_button.show ();
	visible_note_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MidiTrackerEditor::visible_note_press), false);
	toolbar.pack_start (visible_note_button, false, false);

	// Add visible channel button
	visible_channel_button.set_name ("visible channel button");
	visible_channel_button.set_text (S_("Channel|C"));
	visible_channel_button.set_fixed_colors(active_button_color, inactive_button_color);
	visible_channel_button.set_active_state (visible_channel ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	visible_channel_button.show ();
	visible_channel_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MidiTrackerEditor::visible_channel_press), false);
	toolbar.pack_start (visible_channel_button, false, false);

	// Add visible velocity button
	visible_velocity_button.set_name ("visible velocity button");
	visible_velocity_button.set_text (S_("Velocity|V"));
	visible_velocity_button.set_fixed_colors(active_button_color, inactive_button_color);
	visible_velocity_button.set_active_state (visible_velocity ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	visible_velocity_button.show ();
	visible_velocity_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MidiTrackerEditor::visible_velocity_press), false);
	toolbar.pack_start (visible_velocity_button, false, false);

	// Add visible delay button
	visible_delay_button.set_name ("visible delay button");
	visible_delay_button.set_text (S_("Delay|D"));
	visible_delay_button.set_fixed_colors(active_button_color, inactive_button_color);
	visible_delay_button.set_active_state (visible_delay ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	visible_delay_button.show ();
	visible_delay_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MidiTrackerEditor::visible_delay_press), false);
	toolbar.pack_start (visible_delay_button, false, false);

	// Add automation button
	automation_button.set_name ("automation button");
	automation_button.set_text (S_("Automation|A"));
	automation_button.signal_clicked.connect (sigc::mem_fun(*this, &MidiTrackerEditor::automation_click));
	automation_button.show ();
	toolbar.pack_start (automation_button, false, false);

	toolbar.show ();
}

void
MidiTrackerEditor::setup_scroller ()
{
	scroller.add (view);
	scroller.set_policy (POLICY_NEVER, POLICY_AUTOMATIC);
	scroller.show ();
}

void
MidiTrackerEditor::build_beats_per_row_menu ()
{
	using namespace Menu_Helpers;

	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv128 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv128)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv64 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv64)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv32 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv32)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv28 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv28)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv24 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv24)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv20 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv20)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv16 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv16)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv14 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv14)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv12 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv12)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv10 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv10)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv8 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv8)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv7 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv7)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv6 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv6)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv5 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv5)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv4 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv4)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv3 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv3)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeatDiv2 - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeatDiv2)));
	beats_per_row_selector.AddMenuElem (MenuElem ( beats_per_row_strings[(int)SnapToBeat - (int)SnapToBeatDiv128], sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::beats_per_row_selection_done), (SnapType) SnapToBeat)));

	set_size_request_to_display_given_text (beats_per_row_selector, beats_per_row_strings, COMBO_TRIANGLE_WIDTH, 2);
}

void
MidiTrackerEditor::build_automation_action_menu ()
{
	using namespace Menu_Helpers;

	/* detach subplugin_menu from automation_action_menu before we delete automation_action_menu,
	   otherwise bad things happen (see comment for similar case in MidiTimeAxisView::build_automation_action_menu)
	*/

	detach_menu (subplugin_menu);

	_main_automation_menu_map.clear ();
	delete automation_action_menu;
	automation_action_menu = new Menu;

	MenuList& items = automation_action_menu->items();

	automation_action_menu->set_name ("ArdourContextMenu");

	items.push_back (MenuElem (_("Show All Automation"),
	                           sigc::mem_fun (*this, &MidiTrackerEditor::show_all_automation)));

	items.push_back (MenuElem (_("Show Existing Automation"),
	                           sigc::mem_fun (*this, &MidiTrackerEditor::show_existing_automation)));

	items.push_back (MenuElem (_("Hide All Automation"),
	                           sigc::mem_fun (*this, &MidiTrackerEditor::hide_all_automation)));

	/* Attach the plugin submenu. It may have previously been used elsewhere,
	   so it was detached above
	*/

	if (!subplugin_menu.items().empty()) {
		items.push_back (SeparatorElem ());
		items.push_back (MenuElem (_("Processor automation"), subplugin_menu));
		items.back().set_sensitive (true);
	}

	/* Add any route automation */

	// if (gain_track) {
	// 	items.push_back (CheckMenuElem (_("Fader"), sigc::mem_fun (*this, &MidiTrackerEditor::update_gain_track_visibility)));
	// 	gain_automation_item = dynamic_cast<Gtk::CheckMenuItem*> (&items.back ());
	// 	gain_automation_item->set_active ((!for_selection || _editor.get_selection().tracks.size() == 1) &&
	// 	                                  (gain_track && string_is_affirmative (gain_track->gui_property ("visible"))));

	// 	_main_automation_menu_map[Evoral::Parameter(GainAutomation)] = gain_automation_item;
	// }

	// if (trim_track) {
	// 	items.push_back (CheckMenuElem (_("Trim"), sigc::mem_fun (*this, &MidiTrackerEditor::update_trim_track_visibility)));
	// 	trim_automation_item = dynamic_cast<Gtk::CheckMenuItem*> (&items.back ());
	// 	trim_automation_item->set_active ((!for_selection || _editor.get_selection().tracks.size() == 1) &&
	// 	                                  (trim_track && string_is_affirmative (trim_track->gui_property ("visible"))));

	// 	_main_automation_menu_map[Evoral::Parameter(TrimAutomation)] = trim_automation_item;
	// }

	// if (mute_track) {
	// 	items.push_back (CheckMenuElem (_("Mute"), sigc::mem_fun (*this, &MidiTrackerEditor::update_mute_track_visibility)));
	// 	mute_automation_item = dynamic_cast<Gtk::CheckMenuItem*> (&items.back ());
	// 	mute_automation_item->set_active ((!for_selection || _editor.get_selection().tracks.size() == 1) &&
	// 	                                  (mute_track && string_is_affirmative (mute_track->gui_property ("visible"))));

	// 	_main_automation_menu_map[Evoral::Parameter(MuteAutomation)] = mute_automation_item;
	// }

	// if (!pan_tracks.empty()) {
	// 	items.push_back (CheckMenuElem (_("Pan"), sigc::mem_fun (*this, &MidiTrackerEditor::update_pan_track_visibility)));
	// 	pan_automation_item = dynamic_cast<Gtk::CheckMenuItem*> (&items.back ());
	// 	pan_automation_item->set_active ((!for_selection || _editor.get_selection().tracks.size() == 1) &&
	// 					 (!pan_tracks.empty() && string_is_affirmative (pan_tracks.front()->gui_property ("visible"))));

	// 	set<Evoral::Parameter> const & params = _route->pannable()->what_can_be_automated ();
	// 	for (set<Evoral::Parameter>::const_iterator p = params.begin(); p != params.end(); ++p) {
	// 		_main_automation_menu_map[*p] = pan_automation_item;
	// 	}
	// }
}

/** Set up the processor menu for the current set of processors, and
 *  display automation curves for any parameters which have data.
 */
void
MidiTrackerEditor::setup_processor_menu_and_curves ()
{
	_subplugin_menu_map.clear ();
	subplugin_menu.items().clear ();
	route->foreach_processor (sigc::mem_fun (*this, &MidiTrackerEditor::add_processor_to_subplugin_menu));
	// route->foreach_processor (sigc::mem_fun (*this, &RouteTimeAxisView::add_existing_processor_automation_curves));
}

void
MidiTrackerEditor::add_processor_to_subplugin_menu (boost::weak_ptr<ARDOUR::Processor> p)
{
	boost::shared_ptr<ARDOUR::Processor> processor (p.lock ());

	if (!processor || !processor->display_to_user ()) {
		return;
	}

	// /* we use this override to veto the Amp processor from the plugin menu,
	//    as its automation lane can be accessed using the special "Fader" menu
	//    option
	// */

	// if (boost::dynamic_pointer_cast<Amp> (processor) != 0) {
	// 	return;
	// }

	using namespace Menu_Helpers;
	ProcessorAutomationInfo *rai;
	list<ProcessorAutomationInfo*>::iterator x;

	const std::set<Evoral::Parameter>& automatable = processor->what_can_be_automated ();

	if (automatable.empty()) {
		return;
	}

	for (x = processor_automation.begin(); x != processor_automation.end(); ++x) {
		if ((*x)->processor == processor) {
			break;
		}
	}

	if (x == processor_automation.end()) {
		rai = new ProcessorAutomationInfo (processor);
		processor_automation.push_back (rai);
	} else {
		rai = *x;
	}

	/* any older menu was deleted at the top of processors_changed()
	   when we cleared the subplugin menu.
	*/

	rai->menu = manage (new Menu);
	MenuList& items = rai->menu->items();
	rai->menu->set_name ("ArdourContextMenu");

	items.clear ();

	std::set<Evoral::Parameter> has_visible_automation;
	// AutomationTimeAxisView::what_has_visible_automation (processor, has_visible_automation);

	for (std::set<Evoral::Parameter>::const_iterator i = automatable.begin(); i != automatable.end(); ++i) {

		ProcessorAutomationNode* pan;
		Gtk::CheckMenuItem* mitem;

		string name = processor->describe_parameter (*i);

		if (name == X_("hidden")) {
			continue;
		}

		items.push_back (CheckMenuElem (name));
		mitem = dynamic_cast<Gtk::CheckMenuItem*> (&items.back());

		_subplugin_menu_map[*i] = mitem;

		if (has_visible_automation.find((*i)) != has_visible_automation.end()) {
			mitem->set_active(true);
		}

		// if ((pan = find_processor_automation_node (processor, *i)) == 0) {

		// 	/* new item */

		// 	pan = new ProcessorAutomationNode (*i, mitem, *this);

		// 	rai->lines.push_back (pan);

		// } else {

		// 	pan->menu_item = mitem;

		// }

		mitem->signal_toggled().connect (sigc::bind (sigc::mem_fun(*this, &MidiTrackerEditor::processor_menu_item_toggled), rai, pan));
	}

	if (items.size() == 0) {
		return;
	}

	/* add the menu for this processor, because the subplugin
	   menu is always cleared at the top of processors_changed().
	   this is the result of some poor design in gtkmm and/or
	   GTK+.
	*/

	subplugin_menu.items().push_back (MenuElem (processor->name(), *rai->menu));
	rai->valid = true;
}

void
MidiTrackerEditor::processor_menu_item_toggled (MidiTrackerEditor::ProcessorAutomationInfo* rai, MidiTrackerEditor::ProcessorAutomationNode* pan)
{
	bool showit = pan->menu_item->get_active();
	bool redraw = false;

	// if (pan->view == 0 && showit) {
	// 	add_processor_automation_curve (rai->processor, pan->what);
	// 	redraw = true;
	// }

	// if (pan->view && pan->view->set_marked_for_display (showit)) {
	// 	redraw = true;
	// }

	// if (redraw && !no_redraw) {
	// 	request_redraw ();
	// }
}

void
MidiTrackerEditor::setup_tooltips ()
{
	set_tooltip (beats_per_row_selector, _("Beats Per Row"));
	set_tooltip (visible_blank_button, _("Toggle Blank Visibility"));
	set_tooltip (visible_note_button, _("Toggle Note Visibility"));
	set_tooltip (visible_channel_button, _("Toggle Channel Visibility"));
	set_tooltip (visible_velocity_button, _("Toggle Velocity Visibility"));
	set_tooltip (visible_delay_button, _("Toggle Delay Visibility"));
}

void MidiTrackerEditor::set_beats_per_row_to (SnapType st)
{
	unsigned int snap_ind = (int)st - (int)SnapToBeatDiv128;

	string str = beats_per_row_strings[snap_ind];

	if (str != beats_per_row_selector.get_text()) {
		beats_per_row_selector.set_text (str);
	}

	switch (st) {
	case SnapToBeatDiv128: rows_per_beat = 128; break;
	case SnapToBeatDiv64: rows_per_beat = 64; break;
	case SnapToBeatDiv32: rows_per_beat = 32; break;
	case SnapToBeatDiv28: rows_per_beat = 28; break;
	case SnapToBeatDiv24: rows_per_beat = 24; break;
	case SnapToBeatDiv20: rows_per_beat = 20; break;
	case SnapToBeatDiv16: rows_per_beat = 16; break;
	case SnapToBeatDiv14: rows_per_beat = 14; break;
	case SnapToBeatDiv12: rows_per_beat = 12; break;
	case SnapToBeatDiv10: rows_per_beat = 10; break;
	case SnapToBeatDiv8: rows_per_beat = 8; break;
	case SnapToBeatDiv7: rows_per_beat = 7; break;
	case SnapToBeatDiv6: rows_per_beat = 6; break;
	case SnapToBeatDiv5: rows_per_beat = 5; break;
	case SnapToBeatDiv4: rows_per_beat = 4; break;
	case SnapToBeatDiv3: rows_per_beat = 3; break;
	case SnapToBeatDiv2: rows_per_beat = 2; break;
	case SnapToBeat: rows_per_beat = 1; break;
	default:
		/* relax */
		break;
	}

	redisplay_model ();
}

void MidiTrackerEditor::beats_per_row_selection_done (SnapType snaptype)
{
	RefPtr<RadioAction> ract = beats_per_row_action (snaptype);
	if (ract) {
		ract->set_active ();
	}
}

RefPtr<RadioAction>
MidiTrackerEditor::beats_per_row_action (SnapType type)
{
	const char* action = 0;
	RefPtr<Action> act;

	switch (type) {
	case Editing::SnapToBeatDiv128:
		action = "beats-per-row-onetwentyeighths";
		break;
	case Editing::SnapToBeatDiv64:
		action = "beats-per-row-sixtyfourths";
		break;
	case Editing::SnapToBeatDiv32:
		action = "beats-per-row-thirtyseconds";
		break;
	case Editing::SnapToBeatDiv28:
		action = "beats-per-row-twentyeighths";
		break;
	case Editing::SnapToBeatDiv24:
		action = "beats-per-row-twentyfourths";
		break;
	case Editing::SnapToBeatDiv20:
		action = "beats-per-row-twentieths";
		break;
	case Editing::SnapToBeatDiv16:
		action = "beats-per-row-asixteenthbeat";
		break;
	case Editing::SnapToBeatDiv14:
		action = "beats-per-row-fourteenths";
		break;
	case Editing::SnapToBeatDiv12:
		action = "beats-per-row-twelfths";
		break;
	case Editing::SnapToBeatDiv10:
		action = "beats-per-row-tenths";
		break;
	case Editing::SnapToBeatDiv8:
		action = "beats-per-row-eighths";
		break;
	case Editing::SnapToBeatDiv7:
		action = "beats-per-row-sevenths";
		break;
	case Editing::SnapToBeatDiv6:
		action = "beats-per-row-sixths";
		break;
	case Editing::SnapToBeatDiv5:
		action = "beats-per-row-fifths";
		break;
	case Editing::SnapToBeatDiv4:
		action = "beats-per-row-quarters";
		break;
	case Editing::SnapToBeatDiv3:
		action = "beats-per-row-thirds";
		break;
	case Editing::SnapToBeatDiv2:
		action = "beats-per-row-halves";
		break;
	case Editing::SnapToBeat:
		action = "beats-per-row-beat";
		break;
	default:
		fatal << string_compose (_("programming error: %1: %2"), "Editor: impossible beats-per-row", (int) type) << endmsg;
		abort(); /*NOTREACHED*/
	}

	act = ActionManager::get_action (X_("BeatsPerRow"), action);

	if (act) {
		RefPtr<RadioAction> ract = RefPtr<RadioAction>::cast_dynamic(act);
		return ract;

	} else  {
		error << string_compose (_("programming error: %1"), "MidiTrackerEditor::beats_per_row_chosen could not find action to match type.") << endmsg;
		return RefPtr<RadioAction>();
	}
}

void
MidiTrackerEditor::beats_per_row_chosen (SnapType type)
{
	/* this is driven by a toggle on a radio group, and so is invoked twice,
	   once for the item that became inactive and once for the one that became
	   active.
	*/

	RefPtr<RadioAction> ract = beats_per_row_action (type);

	if (ract && ract->get_active()) {
		set_beats_per_row_to (type);
	}
}

void
MidiTrackerEditor::show_all_automation ()
{
	// Copy pasted from route_time_axis.cc

	// if (apply_to_selection) {
	// 	_editor.get_selection().tracks.foreach_route_time_axis (boost::bind (&MidiTrackerEditor::show_all_automation, _1, false));
	// } else {
	// 	no_redraw = true;

	// 	/* Show our automation */

	// 	for (AutomationTracks::iterator i = _automation_tracks.begin(); i != _automation_tracks.end(); ++i) {
	// 		i->second->set_marked_for_display (true);

	// 		Gtk::CheckMenuItem* menu = automation_child_menu_item (i->first);

	// 		if (menu) {
	// 			menu->set_active(true);
	// 		}
	// 	}


	// 	/* Show processor automation */

	// 	for (list<ProcessorAutomationInfo*>::iterator i = processor_automation.begin(); i != processor_automation.end(); ++i) {
	// 		for (vector<ProcessorAutomationNode*>::iterator ii = (*i)->lines.begin(); ii != (*i)->lines.end(); ++ii) {
	// 			if ((*ii)->view == 0) {
	// 				add_processor_automation_curve ((*i)->processor, (*ii)->what);
	// 			}

	// 			(*ii)->menu_item->set_active (true);
	// 		}
	// 	}

	// 	no_redraw = false;

	// 	/* Redraw */

	// 	request_redraw ();
	// }
}

void
MidiTrackerEditor::show_existing_automation ()
{
	// Copy pasted from route_time_axis.cc

	// if (apply_to_selection) {
	// 	_editor.get_selection().tracks.foreach_route_time_axis (boost::bind (&MidiTrackerEditor::show_existing_automation, _1, false));
	// } else {
	// 	no_redraw = true;

	// 	/* Show our automation */

	// 	for (AutomationTracks::iterator i = _automation_tracks.begin(); i != _automation_tracks.end(); ++i) {
	// 		if (i->second->has_automation()) {
	// 			i->second->set_marked_for_display (true);

	// 			Gtk::CheckMenuItem* menu = automation_child_menu_item (i->first);
	// 			if (menu) {
	// 				menu->set_active(true);
	// 			}
	// 		}
	// 	}

	// 	/* Show processor automation */

	// 	for (list<ProcessorAutomationInfo*>::iterator i = processor_automation.begin(); i != processor_automation.end(); ++i) {
	// 		for (vector<ProcessorAutomationNode*>::iterator ii = (*i)->lines.begin(); ii != (*i)->lines.end(); ++ii) {
	// 			if ((*ii)->view != 0 && (*i)->processor->control((*ii)->what)->list()->size() > 0) {
	// 				(*ii)->menu_item->set_active (true);
	// 			}
	// 		}
	// 	}

	// 	no_redraw = false;

	// 	request_redraw ();
	// }
}

void
MidiTrackerEditor::hide_all_automation ()
{
	// Copy pasted from route_time_axis.cc

	// if (apply_to_selection) {
	// 	_editor.get_selection().tracks.foreach_route_time_axis (boost::bind (&MidiTrackerEditor::hide_all_automation, _1, false));
	// } else {
	// 	no_redraw = true;

	// 	/* Hide our automation */

	// 	for (AutomationTracks::iterator i = _automation_tracks.begin(); i != _automation_tracks.end(); ++i) {
	// 		i->second->set_marked_for_display (false);

	// 		Gtk::CheckMenuItem* menu = automation_child_menu_item (i->first);

	// 		if (menu) {
	// 			menu->set_active (false);
	// 		}
	// 	}

	// 	/* Hide processor automation */

	// 	for (list<ProcessorAutomationInfo*>::iterator i = processor_automation.begin(); i != processor_automation.end(); ++i) {
	// 		for (vector<ProcessorAutomationNode*>::iterator ii = (*i)->lines.begin(); ii != (*i)->lines.end(); ++ii) {
	// 			(*ii)->menu_item->set_active (false);
	// 		}
	// 	}

	// 	no_redraw = false;
	// 	request_redraw ();
	// }
}
