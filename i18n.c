/*
 * i18n.c: Internationalization
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: i18n.c,v 1.2 2006-08-07 18:20:43 phintuka Exp $
 *
 * Translations provided by:
 *
 * Finnish   Petri Hintukainen, Rolf Ahrenberg
 * Russian   Vladimir Monchenko
 *
 */

#include <vdr/config.h>
#include "i18n.h"

const tI18nPhrase Phrases[] = {
  {
    "X11/xine-lib output plugin", // English
    "", // Deutsch
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
    "Xine-lib", // English
    "", // Deutsch
    "", // Slovenski
    "", // Italiano
    "", // Nederlands
    "", // Português
    "", // Français
    "", // Norsk
    "Xine-lib", // Suomi
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
  {
    "paused", // English
    "", // Deutsch
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
  {
    "running", // English
    "", // Deutsch
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
  {
    "Interlaced Field Order", // English
    "", // Deutsch
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
  {
    "Decoder state", // English
    "", // Deutsch
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
  {
    "Brightness", // English
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
  {      // min - minutes
    "min", // English
    "", // Deutsch
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
  {    // ms -- milliseconds
    "ms", // English
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "Audio Equalizer >>", // English
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
  {
    "Button$Up", // English
    "", // Deutsch
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
  {
    "Button$Select", // English
    "", // Deutsch
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
  {
    "Button$Info", // English
    "", // Deutsch
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
    "", // Deutsch
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
  {
    "Remove letterbox (4:3 -> 16:9)", // English
    "", // Deutsch
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
  {
    "Play file >>", // English
    "", // Deutsch
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
    "View images >>", // English
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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
    "", // Deutsch
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



