/*
    Copyleft (C) 2015 Nil Geisweiller

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

#ifndef __ardour_gtk2_midi_tracker_matrix_h_
#define __ardour_gtk2_midi_tracker_matrix_h_

#include <gtkmm/treeview.h>
#include <gtkmm/table.h>
#include <gtkmm/box.h>
#include <gtkmm/liststore.h>
#include <gtkmm/scrolledwindow.h>

#include "gtkmm2ext/bindings.h"

#include "evoral/types.hpp"

#include "ardour/session_handle.h"

#include "ardour_dropdown.h"
#include "ardour_window.h"
#include "editing.h"

namespace ARDOUR {
	class MidiRegion;
	class MidiModel;
	class MidiTrack;
	class Session;
};

// TODO: probably split this class into super class TrackerMatrix, so that
// MidiTracker and AutomationMatrix can inherit it.

/**
 * Data structure holding the matrix of events for the tracker
 * representation. Plus some goodies method to generate a tracker matrix given
 * a midi region.
 */
class MidiTrackerMatrix {
public:
	// Holds a note and its associated track number (a maximum of 4096
	// tracks should be more than enough).
	typedef Evoral::Note<Evoral::Beats> NoteType;
	typedef std::multimap<uint32_t, boost::shared_ptr<NoteType> > RowToNotes;
	typedef std::pair<RowToNotes::const_iterator, RowToNotes::const_iterator> NotesRange;

	MidiTrackerMatrix(ARDOUR::Session* session,
	                  boost::shared_ptr<ARDOUR::MidiRegion> region,
	                  boost::shared_ptr<ARDOUR::MidiModel> midi_model,
	                  uint16_t rpb);

	// Set the number of rows per beat. After changing that you probably need
	// to update the matrix, see below.
	void set_rows_per_beat(uint16_t rpb);

	// Build or rebuild the matrix
	void update_matrix();

	// Find the beats corresponding to the first row
	Evoral::Beats find_first_row_beats();

	// Find the beats corresponding to the last row
	Evoral::Beats find_last_row_beats();

	// Find the number of rows of the region
	uint32_t find_nrows();

	// Return the frame at the corresponding row index
	framepos_t frame_at_row(uint32_t irow);

	// Return the beats at the corresponding row index
	Evoral::Beats beats_at_row(uint32_t irow);

	// Return the row index corresponding to the given beats, assuming the
	// minimum allowed delay is -_ticks_per_row/2 and the maximum allowed delay
	// is _ticks_per_row/2.
	uint32_t row_at_beats(Evoral::Beats beats);

	// Return the row index assuming the beats is allowed to have the minimum
	// negative delay (1 - _ticks_per_row).
	uint32_t row_at_beats_min_delay(Evoral::Beats beats);

	// Return the row index assuming the beats is allowed to have the maximum
	// positive delay (_ticks_per_row - 1).
	uint32_t row_at_beats_max_delay(Evoral::Beats beats);

	// Number of rows per beat
	uint8_t rows_per_beat;

	// Determined by the number of rows per beat
	Evoral::Beats beats_per_row;

	// Beats corresponding to the first row
	Evoral::Beats first_beats;

	// Beats corresponding to the last row
	Evoral::Beats last_beats;

	// Number of rows of that region (given the choosen resolution)
	uint32_t nrows;

	// Number of tracker tracks of that midi track (determined by the number of
	// overlapping notes)
	uint16_t ntracks;

	// Map row index to notes on for each track
	std::vector<RowToNotes> notes_on;

	// Map row index to notes off (basically the same corresponding notes on)
	// for each track
	std::vector<RowToNotes> notes_off;

private:
	uint32_t _ticks_per_row;		// number of ticks per rows
	ARDOUR::Session* _session;
	boost::shared_ptr<ARDOUR::MidiRegion> _region;
	boost::shared_ptr<ARDOUR::MidiModel>  _midi_model;
	ARDOUR::BeatsFramesConverter _conv;	
};

#endif /* __ardour_gtk2_midi_tracker_matrix_h_ */
