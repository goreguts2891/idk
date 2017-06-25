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

#include <vector>

#include <gtkmm/spinbutton.h>

#include "gtkmm2ext/utils.h"
#include "pbd/i18n.h"

#include "jump_distance_widget.h"


using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ArdourSurface;

JumpDistanceWidget::JumpDistanceWidget (JumpDistance dist)
	: HBox ()
	, _distance (dist)
	, _value_adj (dist.value, 0, 100, 0.25)
{
	SpinButton* sb = manage (new SpinButton (_value_adj, 0.25, 2));
	sb->signal_value_changed().connect (boost::bind (&JumpDistanceWidget::update_value, this));
	pack_start (*sb);

	const vector<string> jog_units_strings ({ _("seconds"), _("beats"), _("bars") });
	set_popdown_strings (_unit_cb, jog_units_strings);
	_unit_cb.set_active(_distance.unit);
	_unit_cb.signal_changed().connect (boost::bind (&JumpDistanceWidget::update_unit, this));
	pack_start (_unit_cb);
}

void
JumpDistanceWidget::update_unit ()
{
	_distance.unit = JumpUnit (_unit_cb.get_active_row_number ());
	Changed (); /* emit signal */
}

void
JumpDistanceWidget::update_value ()
{
	_distance.value =  _value_adj.get_value ();
	Changed (); /* emit signal */
}
