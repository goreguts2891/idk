
/* This file was generated by tools/process-metadata. DO NOT EDIT THIS FILE. EVER! */
void
UIConfiguration::build_metadata ()
{

#define VAR_META(name,...)  { char const * _x[] { __VA_ARGS__ }; all_metadata.insert (std::make_pair<std::string,Metadata> ((name), PBD::upcase (_x))); }

	VAR_META (X_("blink-rec-arm"), _("appearance"), _("blink"), _("record"), _("rec"), _("enable"), _("rec-enable"), _("record-enable"),  NULL);
	VAR_META (X_("boxy-buttons"), _("appearance"), _("style"), _("boxy"), _("buttons"), _("theme"),  NULL);
	VAR_META (X_("buggy-gradients"), _("appearance"), _("bugs"), _("tweaks"), _("kwirks"),  NULL);
	VAR_META (X_("check-announcements"), _("check"), _("announcements"), _("phone"), _("home"),  NULL);
	VAR_META (X_("clock-display-limit"), _("clock"), _("display"), _("limit"), _("length"), _("maximum"), _("duration"),  NULL);
	VAR_META (X_("color-file"), _("theme"), _("colors"), _("appearance"), _("style"), _("themeing"),  NULL);
	VAR_META (X_("color-regions-using-track-color"), _("theme"), _("colors"), _("appearance"), _("style"), _("themeing"),  NULL);
	VAR_META (X_("default-bindings"), _("shortcuts"), _("keys"), _("keybindings"), _("bindings"),  NULL);
	VAR_META (X_("default-lower-midi-note"), _("MIDI"), _("low"), _("lowest"), _("lower"),  NULL);
	VAR_META (X_("default-narrow_ms"), _("appearance"), _("width"), _("mixer"),  NULL);
	VAR_META (X_("default-upper-midi-note"), _("MIDI"), _("upper"), _("high"), _("highest"),  NULL);
	VAR_META (X_("draggable-playhead"), _("playhead"), _("drag"), _("dragging"),  NULL);
	VAR_META (X_("draggable-playhead-speed"), _("playhead"), _("drag"), _("dragging"),  NULL);
	VAR_META (X_("editor-stereo-only-meters"), _("editor"), _("stereo"), _("meters"),  NULL);
	VAR_META (X_("flat-buttons"), _("appearance"), _("style"), _("flat"), _("buttons"), _("theme"), _("ableton"),  NULL);
	VAR_META (X_("floating-monitor-section"), _("appearance"), _("monitor"), _("floating"), _("separate"), _("window"),  NULL);
	VAR_META (X_("font-scale"), _("fonts"), _("font"), _("size"), _("scaling"), _("readable"), _("readability"),  NULL);
	VAR_META (X_("freesound-dir"), _("freesound"), _("folder"), _("folders"), _("directory"), _("directories"), _("download"),  NULL);
	VAR_META (X_("hide-splash-screen"), _("appearance"), _("hide"), _("splash"), _("screen"),  NULL);
	VAR_META (X_("afl-position"), _("monitoring"), _("monitor"), _("afl"), _("pfl"), _("pre"), _("post"), _("position"),  NULL);
	VAR_META (X_("auto-analyse-audio"), _("automatic"), _("automated"), _("audio"), _("analysis"), _("transients"),  NULL);
	VAR_META (X_("click-sound"), _("metronome"), _("click"), _("beat"), _("sound"), _("sample"),  NULL);
	VAR_META (X_("clip-library-dir"), _("folder"), _("folders"), _("directory"), _("directories"), _("download"), _("clips"), _("library"),  NULL);
	VAR_META (X_("cpu-dma-latency"), _("cpu"), _("dma"), _("latency"), _("performance"), _("xrun"),  NULL);
	VAR_META (X_("create-xrun-marker"), _("xrun"), _("xmarker"),  NULL);

}
