/*
 * i18n.c: Internationalization
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: i18n.c,v 1.12 2006-12-24 09:41:53 phintuka Exp $
 *
 * Translations provided by:
 *
 * Finnish   Petri Hintukainen, Rolf Ahrenberg
 * Russian   Vladimir Monchenko
 * German    Udo Richter
 *
 */

#include <vdr/config.h>
#include "i18n.h"

const tI18nPhrase Phrases[] = {
  {
    "X11/xine-lib output plugin", // English
    "X11/xine-lib Ausgabe-Plugin", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "X11/xine-lib näyttölaite", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "X11/xine-lib ÒØÔÕŞ ÜŞÔãÛì", // Russian
    "", // Croatian
  },
  {
    "Media Player (xine-lib)", // English
    "Xine-lib", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "Mediasoitin (xine-lib)", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "Xine-lib", // Russian
    "", // Croatian
  },
  {
    "high", // English
    "Hoch", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "korkea", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "²ëáŞÚØÙ", // Russian
    "", // Croatian
  },
  {
    "low", // English
    "Niedrig", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "matala", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "½Ø×ÚØÙ", // Russian
    "", // Croatian
  },
  {
    "normal", // English
    "Normal", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "normaali", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "½ŞàÜĞÛìİëÙ", // Russian
    "", // Croatian
  },
  {
    "inverted", // English
    "Invertiert", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "käänteinen", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "¸İÒÕàâØàŞÒĞİŞ", // Russian
    "", // Croatian
  },
#if 0
  {
    "paused", // English
    "Pausiert", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "pysäytetty", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "¿àØŞáâĞİŞÒÛÕİ", // Russian
    "", // Croatian
  },
#endif
#if 0
  {
    "running", // English
    "Laufend", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "toiminnassa", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "·ĞßãéÕİ", // Russian
    "", // Croatian
  },
#endif
  {
    "Interlaced Field Order", // English
    "Interlaced Halbbild-Reihenfolge", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "Lomitettujen kenttien järjestys", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "ÇÕàÕ×áâàŞçİëÙ ßŞàïÔŞÚ ßŞÛÕÙ", // Russian
    "", // Croatian
  },
#if 0
  {
    "Decoder state", // English
    "Dekoder-Status", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "Dekooderin tila", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "ÁŞáâŞïİØÕ ÔÕÚŞÔÕàĞ", // Russian
    "", // Croatian
  },
#endif
  {
    "Brightness", // English
    "Helligkeit", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "Kirkkaus", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "ÏàÚŞáâì", // Russian
    "", // Croatian
  },
  {
    "Decoder", // English
    "Dekoder", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "Dekooderi", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "´ÕÚŞÔÕà", // Russian
    "", // Croatian
  },
  {
    "Audio", // English
    "Audio", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "Ääni", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "°ãÔØŞ", // Russian
    "", // Croatian
  },
  {
    "On-Screen Display", // English
    "On-Screen Display", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "Kuvaruutunäyttö", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "ÍÚàĞİİŞÕ ÜÕİî", // Russian
    "", // Croatian
  },
  {
    "Hide main menu", // English
    "Verstecke Hauptmenü", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "Piilota valinta päävalikossa", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "ÁÚàëâì ŞáİŞÒİŞÕ ÜÕİî", // Russian
    "", // Croatian
  },
  {
    "Window aspect", // English
    "Fenster-Seitenverhältnis", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "Ikkunan kuvasuhde", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "ÁŞŞâİŞèÕİØÕ áâŞàŞİ", // Russian
    "", // Croatian
  },
  {
    "Scale to window size", // English
    "Skaliere auf Fenster-Größe", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "Skaalaa ikkunan kokoiseksi", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "¼ĞáèâĞÑØàŞÒĞâì Ò àĞ×ÜÕà ŞÚİĞ", // Russian
    "", // Croatian
  },
  {
    "Scale OSD to video size", // English
    "Skaliere OSD auf Videogröße", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "Skaalaa videon kokoiseksi", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "¼ĞáèâĞÑØàŞÒĞâì Ò àĞ×ÜÕà ÒØÔÕŞ", // Russian
    "", // Croatian
  },
  {
    "Unscaled OSD (no transparency)", // English
    "Unskaliertes OSD (keine Transparenz)", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "Skaalaamaton (ei läpinäkyvyyttä)", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "½Õ ÜĞáèâĞÑØàŞÒĞâì (İÕ ßàŞ×àĞçİŞ)", // Russian
    "", // Croatian
  },
  {
    "Dynamic transparency correction", // English
    "Dynamische Transparenz-Korrektur", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "Dynaaminen läpinäkyvyyden korjaus", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "´ØİĞÜØçÕáÚĞï ÚŞààÕÚæØï ßàŞ×àĞçİŞáâØ", // Russian
    "", // Croatian
  },
  {
    "Static transparency correction", // English
    "Statische Transparenz-Korrektur", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "Läpinäkyvyyden korjaus", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "ÁâĞâØçÕáÚĞï ÚŞààÕÚæØï ßàŞ×àĞçİŞáâØ", // Russian
    "", // Croatian
  },
  {
    "Video", // English
    "Video", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "Kuva", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "²ØÔÕŞ", // Russian
    "", // Croatian
  },
  {
    "Deinterlacing", // English
    "De-Interlacing", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "Lomituksen poisto", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "´ÕØİâÕàÛÕÙáØİÓ", // Russian
    "", // Croatian
  },
  {
    "Remote Clients", // English
    "Entfernte Clients", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "Etäkäyttö", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "ÃÔĞÛÕİİëÕ ÚÛØÕİâë", // Russian
    "", // Croatian
  },
  {
    "Allow remote clients", // English
    "Erlaube entfernte Clients", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "Salli etäkäyttö", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "ÀĞ×àÕèØâì ãÔĞÛÕİİëå ÚÛØÕİâŞÒ", // Russian
    "", // Croatian
  },
  {
    "  Listen port (TCP and broadcast)", // English
    "  Empfangender Port (TCP und Broadcast)", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "  Kuuntele TCP-porttia", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "  ¿Şàâ (TCP Ø èØàŞÚŞÒÕèĞâÕÛìİëÙ)", // Russian
    "", // Croatian
  },
  {
    "  Remote keyboard", // English
    "  Tastaturfernsteuerung", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "  Käytä etänäppäimistöä", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "  ÃÔĞÛÕİİĞï ÚÛĞÒØĞâãàĞ", // Russian
    "", // Croatian
  },
  {
    "Buffer size", // English
    "Puffergröße", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "Puskurin koko", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "ÀĞ×ÜÕà ÑãäÕàĞ", // Russian
    "", // Croatian
  },
  {
    "  Number of PES packets", // English
    "  Anzahl PES-Pakete", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "  PES-pakettien lukumäärä", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "  PES ßĞÚÕâŞÒ", // Russian
    "", // Croatian
  },
  {
    "Priority", // English
    "Priorität", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "Prioriteetti", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "¿àØŞàØâÕâ", // Russian
    "", // Croatian
  },
  {
    "custom", // English
    "Benutzerdefiniert", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "oma", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "¿ŞÛì×ŞÒĞâÕÛì", // Russian
    "", // Croatian
  },
  {
    "tiny", // English
    "Winzig", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "olematon", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "¾çÕİì ÜĞÛÕİìÚØÙ", // Russian
    "", // Croatian
  },
  {
    "small", // English
    "Klein", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "pieni", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "¼ĞÛÕİìÚØÙ", // Russian
    "", // Croatian
  },
  {
    "medium", // English
    "Mittel", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "keskikokoinen", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "ÁàÕÔİØÙ", // Russian
    "", // Croatian
  },
  {
    "large", // English
    "Groß", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "suuri", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "±ŞÛìèŞÙ", // Russian
    "", // Croatian
  },
  {
    "huge", // English
    "Riesig", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "valtava", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "¾çÕİì ÑŞÛìİŞÙ", // Russian
    "", // Croatian
  },
  {
    "Display address", // English
    "Bildschirm-Adresse", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "Näytön osoite", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "°ÔàÕá ÔØáßÛÕï", // Russian
    "", // Croatian
  },
  {
    "Use keyboard", // English
    "Tastatur benutzen", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "Käytä näppäimistöä", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "¸áßŞÛì×ŞÒĞâì ÚÛĞÒØĞâãàã", // Russian
    "", // Croatian
  },
  {
    "Driver", // English
    "Treiber", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "Ohjain", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "´àĞÙÒÕà", // Russian
    "", // Croatian
  },
  {
    "Port", // English
    "Port", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "Portti", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "¿Şàâ", // Russian
    "", // Croatian
  },
  {
    "Delay", // English
    "Verzögerung", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "Viive", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "·ĞÔÕàÖÚĞ", // Russian
    "", // Croatian
  },
#if 0
  {      // min - minutes
    "min", // English
    "min", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "min", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "min", // Russian
    "", // Croatian
  },
#endif
  {    // ms -- milliseconds
    "ms", // English
    "ms", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "ms", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "ms", // Russian
    "", // Croatian
  },
  {     // px - pixels
    "px", // English
    "px", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "px", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "ßØÚáÕÛÕÙ", // Russian
    "", // Croatian
  },
  {
    "  Window width", // English
    "  Fensterbreite", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "  Ikkunan leveys", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "  ÈØàØİĞ ŞÚİĞ", // Russian
    "", // Croatian
  },
  {
    "  Window height", // English
    "  Fensterhöhe", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "  Ikkunan korkeus", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "  ²ëáŞâĞ ŞÚİĞ", // Russian
    "", // Croatian
  },
  {
    "automatic", // English
    "Automatik", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "automaattinen", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "°ÒâŞÜĞâØçÕáÚØ", // Russian
    "", // Croatian
  },
  {
    "default", // English
    "Standard", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "oletus", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "¿Ş ãÜŞÛçĞİØî", // Russian
    "", // Croatian
  },
  {
    "4:3", // English
    "4:3", // Deutsch
    "4:3", // Slovenski
    "4:3", // Italiano
    "4:3", // Nederlands
    "4:3", // Português
    "4:3", // Français
    "4:3", // Norsk
    "4:3", // Suomi
    "4:3", // Polski
    "4:3", // Español
    "4:3", // Ellinika
    "4:3", // Svenska
    "4:3", // Romaneste
    "4:3", // Magyar
    "4:3", // Catala
    "4:3", // Russian
    "4:3", // Croatian
  },
  {
    "16:9", // English
    "16:9", // Deutsch
    "16:9", // Slovenski
    "16:9", // Italiano
    "16:9", // Nederlands
    "16:9", // Português
    "16:9", // Français
    "16:9", // Norsk
    "16:9", // Suomi
    "16:9", // Polski
    "16:9", // Español
    "16:9", // Ellinika
    "16:9", // Svenska
    "16:9", // Romaneste
    "16:9", // Magyar
    "16:9", // Catala
    "16:9", // Russian
    "16:9", // Croatian
  },
  {
    "Pan&Scan", // English
    "Pan&Scan", // Deutsch
    "Pan&Scan", // Slovenski
    "Pan&Scan", // Italiano
    "Pan&Scan", // Nederlands
    "Pan&Scan", // Português
    "Pan&Scan", // Français
    "Pan&Scan", // Norsk
    "Pan&Scan", // Suomi
    "Pan&Scan", // Polski
    "Pan&Scan", // Español
    "Pan&Scan", // Ellinika
    "Pan&Scan", // Svenska
    "Pan&Scan", // Romaneste
    "Pan&Scan", // Magyar
    "Pan&Scan", // Catala
    "Pan&Scan", // Russian
    "Pan&Scan", // Croatian
  },
  {
    "HUE", // English
    "Farbton", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "Värisävy", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "HUE", // Russian
    "", // Croatian
  },
  {
    "Saturation", // English
    "Sättigung", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "Saturaatio", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "½ĞáëéÕİİŞáâì", // Russian
    "", // Croatian
  },
  {
    "Contrast", // English
    "Kontrast", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "Kontrasti", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "ºŞİâàĞáâİŞáâì", // Russian
    "", // Croatian
  },
  {
    "off", // English
    "Aus", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "ei käytössä", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "²ëÚÛ.", // Russian
    "", // Croatian
  },
  {
    "no audio", // English
    "Kein Audio", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "ei ääntä", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "½Õâ ĞãÔØŞ", // Russian
    "", // Croatian
  },
  {
    "no video", // English
    "Kein Video", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "ei kuvaa", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "½Õâ ÒØÔÕŞ", // Russian
    "", // Croatian
  },
  {
    "Fullscreen mode", // English
    "Vollbild-Modus", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "Kokoruututila", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "¿ŞÛİŞíÚàĞİİëÙ àÕÖØÜ", // Russian
    "", // Croatian
  },
  {
    "Local Frontend", // English
    "Lokale Anzeige", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "Paikallinen näyttö", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "»ŞÚĞÛìİëÙ äàŞİâÕİÔ", // Russian
    "", // Croatian
  },
  {
    "Local Display Frontend", // English
    "Lokale Bildschirmanzeige", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "Paikallinen näyttö", // Suomi
    "", // Polski
    "", // Español
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "ÄàŞİâÕİÔ ÛŞÚĞÛìİŞÓŞ íÚàĞİĞ", // Russian
    "", // Croatian
  },
  {
    "Delete image ?", // English
    "Bild löschen?", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Poistetaanko kuva ?", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "ÃÔĞÛØâì ÚĞàâØİÚã ?", // Russian
    "", // Croatian
  },
  {
    "  TCP transport", // English
    "  TCP-Übertragung", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "  TCP-siirto", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "TCP âàĞİáßŞàâ", // Russian
    "", // Croatian
  },
  {
    "  UDP transport", // English
    "  UDP-Übertragung", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "  UDP-siirto", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "UDP âàĞİáßŞàâ", // Russian
    "", // Croatian
  },
  {
    "  RTP (multicast) transport", // English
    "  RTP (multicast) Übertragung", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "  RTP (multicast) -siirto", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "  RTP (èØàŞÚŞÒÕéĞâÕÛìİëÙ) âàĞİáßŞàâ", // Russian
    "", // Croatian
  },
  {
    "  PIPE transport", // English
    "  Pipe-Übertragung", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "  PIPE-siirto", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "  PIPE âàĞİáßŞàâ", // Russian
    "", // Croatian
  },
  {
    "  Server announce broadcasts", // English
    "  Server-Bekanntmachung Broadcast", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "  Palvelimen broadcast-ilmoitukset", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "  ÁÕàÒÕà ØáßŞÛì×ãÕâ èØàŞÚŞÒÕéĞİØÕ", // Russian
    "", // Croatian
  },
  {
    "Audio equalizer >>", // English
    "Audio-Equalizer >>", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Taajuuskorjain >>", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "°ãÔØŞ íÚÒĞÛĞÙ×Õà >>", // Russian
    "", // Croatian
  },
  {
    "Audio Equalizer", // English
    "Audio Equalizer", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Taajuuskorjain", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "°ãÔØŞ íÚÒĞÛĞÙ×Õà", // Russian
    "", // Croatian
  },
  {
    "Grayscale", // English
    "Graustufen", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Harmaasävy", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "¾ââÕİÚØ áÕàŞÓŞ", // Russian
    "", // Croatian
  },
  {
    "Bitmap", // English
    "Bitmap", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Bittikartta", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "±ØâŞÒĞï ÚĞàâĞ", // Russian
    "", // Croatian
  },
#if 0
  {
    "Button$Up", // English
    "Hoch", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Edellinen", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
#endif
#if 0
  {
    "Button$Select", // English
    "Auswahl", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Valitse", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
#endif
  {
    "Button$Info", // English
    "Info", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Tiedot", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "Audio Compression", // English
    "Audio-Komprimierung", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Voimista hiljaisia ääniä", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "°ãÔØŞ ÚŞÜßàÕááØï", // Russian
    "", // Croatian
  },
#if 0
  {
    "Remove letterbox (4:3 -> 16:9)", // English
    "Entferne Letterbox (4:3 -> 16:9)", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Poista letterbox (4:3 -> 16:9)", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "ÃÔĞÛØâì letterbox (4:3 -> 16:9)", // Russian
    "", // Croatian
  },
#endif
  {
    "Play file >>", // English
    "Datei abspielen >>", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Toista tiedosto >>", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "¿àŞØÓàĞâì äĞÙÛ >>", // Russian
    "", // Croatian
  },
  {
    "Play music >>", // English
    "Musik abspielen >>", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Toista musiikkia >>", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "¿àŞØÓàĞâì äĞÙÛ >>", // Russian
    "", // Croatian
  },
  {
    "View images >>", // English
    "Bilder ansehen >>", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Katsele kuvia >>", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "¿àŞáÜŞâàÕâì Ø×ŞÑàĞÖÕİØï >>", // Russian
    "", // Croatian
  },
  {
    "Play file", // English
    "Datei abspielen", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Toista tiedosto", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "¿àŞØÓàĞâì äĞÙÛ", // Russian
    "", // Croatian
  },
  {
    "Images", // English
    "Bilder", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Kuvat", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "¸×ŞÑàĞÖÕİØï", // Russian
    "", // Croatian
  },
  {
    "CenterCutOut", // English
    "CenterCutOut", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "CenterCutOut", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "Test Images", // English
    "Testbilder", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Testikuvat", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "ÂÕáâŞÒëÕ Ø×ŞÑàĞÖÕİØï", // Russian
    "", // Croatian
  },
  {
    "Visualization", // English
    "Visualisierung", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Visualisointi", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "²Ø×ãĞÛØ×ĞæØï", // Russian
    "", // Croatian
  },
  {
    "Upmix stereo to 5.1", // English
    "Stereo zu 5.1 hoch mischen", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Miksaa stereo 5.1-kanavaiseksi", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "¿àÕŞÑàĞ×ŞÒĞâì áâÕàÕŞ Ò 5.1", // Russian
    "", // Croatian
  },
  {
    "Downmix AC3 to surround", // English
    "AC3 zu Surround herunter mischen", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Miksaa AC3 surroundiksi", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "Framebuffer device", // English
    "Framebuffer-Device", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Framebuffer-laite", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "Framebuffer ãáâàŞÙáâÒŞ", // Russian
    "", // Croatian
  },
  {
    "  Allow downscaling", // English
    "  Verkleinern zulassen", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "  Salli skaalaus pienemmäksi", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "  ¼ĞáèâĞÑØàŞÒĞâì á ßĞÔÕİØÕÜ ÚĞçÕáâÒĞ", // Russian
    "", // Croatian
  },
  {
    "  When opaque OSD", // English
    "  Wenn undurchsichtiges OSD", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "  Kun ei läpinäkyvä", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "  ºŞÓÔĞ İÕßàŞ×àĞçİŞ OSD", // Russian
    "", // Croatian
  },
  {
    "  When low-res video", // English
    "  Wenn Video mit niedriger Auflösung", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "  Kun matalaresoluutioinen video", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "  ºŞÓÔĞ ÒØÔÕŞ İØ×ÚŞÓŞ àĞ×àÕèÕİØï", // Russian
    "", // Croatian
  },

  // 1.0.0pre2:
  {
    "Play remote DVD >>", // English
    "Spiele Fern-DVD >>", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Toista DVD etäkoneesta >>", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "Play DVD Disc >>", // English
    "Spiele DVD-Disk", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Toista DVD >>", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "Crop letterbox 4:3 to 16:9", // English
    "Schneide letterbox 4:3 zu 16:9", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Leikkaa 4:3 letterbox 16:9:ksi", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "Play only audio", // English
    "Nur Audio spielen", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Toista pelkkä ääni", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "Off", // English
    "Aus", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "ei käytössä", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "OSS", // English
    "OSS", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "OSS", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "Alsa", // English
    "Alsa", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Alsa", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "Goom", // English
    "Goom", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Goom", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "Oscilloscope", // English
    "Oszilloskop", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Oskilloskooppi", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "FFT Scope", // English
    "FFT Spektrum", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Spektri", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "FFT Graph", // English
    "FFT Graph", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Spektrogrammi", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "X11 (sxfe)", // English
    "X11 (sxfe)", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "X11 (sxfe)", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "Framebuffer (fbfe)", // English
    "Framebuffer (fbfe)", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Framebuffer (fbfe)", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "Xv", // English
    "Xv", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Xv", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "XShm", // English
    "XShm", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "XShm", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "Bob", // English
    "Bob", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Bob", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "Weave", // English
    "Weave", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Weave", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "Greedy", // English
    "Greedy", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Greedy", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "One Field", // English
    "Ein Halbbild", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "One Field", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "One Field XV", // English
    "Ein Halbbild XV", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "One Field XV", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "Linear Blend", // English
    "Linear mischen", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Linear Blend", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "TvTime", // English
    "TvTime", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "TvTime", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "   Options", // English
    "   Optionen", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "   Parametrit", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "    Multicast address", // English
    "    Multicast-Adresse", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "    Multicast osoite", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "    Multicast port", // English
    "    Multicast-Port", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "    Multicast portti", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "    Multicast TTL", // English
    "    Multicast-TTL", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "    Multicast TTL", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "    Transmit always on", // English
    "    Immer senden", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "    Multicast-lähetys aina päällä", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "Speakers", // English
    "Lautsprecher", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Kaiuttimet", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "Headphones 2.0", // English
    "Kopfhöhrer 2.0", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Kuulokkeet 2.0", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "  Autodetect letterbox", // English
    "  Letterbox automatisch erkennen", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "  Tunnista letterbox automaattisesti", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "  Soft start", // English
    "  Weich starten", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "  Portaittainen aloitus", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "  Crop to", // English
    "  Schneide auf", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "  Leikkaa kokoon", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "  Detect subtitles", // English
    "  Erkenne Untertitel", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "  Huomioi tekstitys", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
   "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },

  // 1.0.0pre4:
  {
    "Media", // English
    "Medien", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Media", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "Video settings", // English
    "Video-Einstellungen", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Videoasetukset", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "Audio settings", // English
    "Audio-Einstellungen", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Ääniasetukset", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "Overscan (crop image borders)", // English
    "Overscan (Bildränder abschneiden)", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Overscan (leikkaa kuvasta reunat)", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  // Missing texts 2006-09-20
  {
    "Post processing (ffmpeg)", // English
    "", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  { // ffmpeg post processing
    "  Quality", // English
    "", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "  Laatu", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  { // ffmpeg post processing
    "  Mode", // English
    "", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  { // tvtime de-interlacing
    "     Method", // English
    "", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "     Menetelmä", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  { // tvtime de-interlacing
    "     Cheap mode", // English
    "", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  { // tvtime de-interlacing
    "     Pulldown", // English
    "", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  { // tvtime de-interlacing
    "     Frame rate", // English
    "", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  { // tvtime de-interlacing
    "     Judder Correction", // English
    "", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  { // tvtime de-interlacing
    "     Use progressive frame flag", // English
    "", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  { // tvtime de-interlacing
    "     Chroma Filter", // English
    "", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
#if 0
  {  // in vdr i18n.c
    "Delete recording?",
    "Aufzeichnung löschen?",
    "Izbri¹i posnetek?",
    "Cancellare la registrazione?",
    "Opname verwijderen?",
    "Apagar a gravação?",
    "Supprimer l'enregistrement?",
    "Slette opptak?",
    "Poistetaanko tallenne?",
    "Usun±æ nagranie?",
    "¿Eliminar grabacion?",
    "ÄéáãñáöŞ åããñáöŞò?",
    "Ta bort inspelningen?",
    "ªterg înregistrarea?",
    "Felvétel törlése?",
    "Esborrar gravació?",
    "ÁâÕàÕâì ×ĞßØáì?",
    "Obrisati snimku?",
    "Kustutada salvestus?",
    "Slet optagelse?",
    "Smazat nahrávku?",
  },
#endif
#if 0
  { // in vdr i18n.c
    "Error while deleting recording!",
    "Fehler beim Löschen der Aufzeichnung!",
    "Napaka pri brisanju posnetka!",
    "Errore nel cancellare la registrazione!",
    "Fout bij verwijderen opname!",
    "Erro enquanto apagava uma gravação!",
    "Erreur de suppression de l'enregistrement!",
    "Feil under sletting av opptak!",
    "Tallenteen poistaminen epäonnistui!",
    "Bl±d podczas usuwania nagrania!",
    "¡Error al borrar la grabación!",
    "ËÜèïò êáôÜ ôŞí äéáãñáöŞ ôïõ áñ÷åßïõ!",
    "Inspelningen går inte att ta bort!",
    "Eroare la ºtergerea înregistrãrii!",
    "Hiba a felvétel törlésénél!",
    "Error a l'esborrar la gravació!",
    "¾èØÑÚĞ ãÔĞÛÕİØï ×ĞßØáØ!",
    "Gre¹ka pri brisanju snimke!",
    "Salvestuse kustutamine ebaõnnestus!",
    "Fejl ved sletning af optagelse!",
    "Chyba pøi mazání nahrávky!",
  },
#endif
  {
    "Select DVD SPU Track", // English
    "", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Valitse tekstityskieli (DVD)", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "  Select DVD SPU track >>", // English
    "", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "  Valitse tekstityskieli (DVD) >>", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "DVD SPU Track", // English
    "", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "DVD-tekstityskieli", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "Aspect ratio", // English
    "", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Kuvasuhde", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
#if 0
  {
    "Headphone audio mode", // English
    "", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
#endif
#if 0
  {
    "Mix to headphones", // English
    "", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
#endif
  {
    "Play music", // English
    "", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Toista musiikkia", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "Now playing", // English
    "", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "Random play", // English
    "", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Satunnaistoisto", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "Normal play", // English
    "", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Normaali toisto", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "Frontend initialization failed", // English
    "", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Näyttölaitteen alustus epäonnistui", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "Server initialization failed", // English
    "", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Palvelimen käynnistys epäonnistui", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  // Goom options
  {
    "  Width", // English
    "", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "  Leveys", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "  Height", // English
    "", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "  Korkeus", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "  Speed", // English
    "", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "  Nopeus", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "    SAP announcements", // English
    "", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "    SAP multicast-ilmoitukset", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "Play remote CD >>", // English
    "", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Toista CD etäkoneesta >>", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
  {
    "Play audio CD >>", // English
    "", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "Toista CD >>", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },

#if 0
  {
    "", // English
    "", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Portugu<EA>s
    "", // Fran<E7>ais
    "", // Norsk
    "", // Suomi
    "", // Polski
    "", // Espa<F1>ol
    "", // Ellinika
    "", // Svenska
    "", // Romaneste
    "", // Magyar
    "", // Catala
    "", // Russian
    "", // Croatian
  },
#endif

  { NULL }
};



