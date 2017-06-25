/*
    Copyright (C) 2009-2013 Paul Davis
    Author: Johannes Mueller

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

#ifndef ardour_shuttlepro_button_config_widget_h
#define ardour_shuttlepro_button_config_widget_h

#include <gtkmm/box.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/combobox.h>
#include <gtkmm/treestore.h>

#include "pbd/signals.h"

#include "shuttlepro.h"
#include "jump_distance_widget.h"

namespace ArdourSurface
{
class ButtonConfigWidget : public Gtk::HBox
{
public:
	ButtonConfigWidget ();
	~ButtonConfigWidget () {};

	void set_current_config (boost::shared_ptr<ButtonBase> btn_cnf);
	boost::shared_ptr<ButtonBase> get_current_config (ShuttleproControlProtocol& scp) const;

	PBD::Signal0<void> Changed;

private:
	void set_current_action (std::string action_string);
	void set_jump_distance (JumpDistance dist);

	Gtk::RadioButton _choice_jump;
	Gtk::RadioButton _choice_action;

	void update_choice ();
	void update_config ();

	bool find_action_in_model (const Gtk::TreeModel::iterator& iter, std::string const& action_path, Gtk::TreeModel::iterator* found);

	JumpDistanceWidget _jump_distance;
	Gtk::ComboBox _action_cb;

	struct ActionColumns : public Gtk::TreeModel::ColumnRecord {
		ActionColumns() {
			add (name);
			add (path);
		}
		Gtk::TreeModelColumn<std::string> name;
		Gtk::TreeModelColumn<std::string> path;
	};

	ActionColumns _action_columns;
	Glib::RefPtr<Gtk::TreeStore> _available_action_model;
	std::map<std::string,std::string> _action_map; // map from action names to paths
	void setup_available_actions ();

	PBD::ScopedConnection _jump_distance_connection;
};
}

#endif /* ardour_shuttlepro_button_config_widget_h */
;
