/*
 * menuitems.c: New menu item types
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: menuitems.c,v 1.1 2006-06-03 10:01:17 phintuka Exp $
 *
 */

#include <vdr/i18n.h>

#include "menuitems.h"

// --- cMenuEditTypedIntItem -------------------------------------------------

cMenuEditTypedIntItem::cMenuEditTypedIntItem(const char *Name, const char *Type, int *Value, int Min, int Max, const char *ZeroText)
:cMenuEditIntItem(Name,Value,Min,Max)
{
  type = strdup(Type?Type:"");
  zeroText = ZeroText ? strdup(ZeroText) : NULL;
  Set();
}

cMenuEditTypedIntItem::~cMenuEditTypedIntItem()
{
  free(type);
  if(zeroText)
    free(zeroText);
}

void cMenuEditTypedIntItem::Set(void)
{
  char buf[16];
  if(zeroText && *value == 0) {
    SetValue(zeroText);
  } else {
    snprintf(buf, sizeof(buf), "%d %s", *value, type);
    SetValue(buf);
  }
}

// --- cMenuEditStraI18nItem -------------------------------------------------

cMenuEditStraI18nItem::cMenuEditStraI18nItem(const char *Name, int *Value, int NumStrings, const char * const *Strings)
:cMenuEditIntItem(Name, Value, 0, NumStrings - 1)
{
  strings = Strings;
  Set();
}

void cMenuEditStraI18nItem::Set(void)
{
  SetValue(tr(strings[*value]));
}

// --- cFileListItem -------------------------------------------------

cFileListItem::cFileListItem(const char *name, bool isDir)
{
  m_Name = strdup(name);
  m_IsDir = isDir;
  m_HasResume = false;
  m_HasSubs   = false;
  m_ShowFlags = false;
  m_Up = m_IsDir && !strcmp(m_Name,"..");
  Set();
}

cFileListItem::cFileListItem(const char *name, bool isDir, bool HasResume, bool HasSubs)
{
  m_Name = strdup(name);
  m_IsDir = isDir;
  m_HasResume = HasResume;
  m_HasSubs   = HasSubs;
  m_ShowFlags = true;
  m_Up = m_IsDir && !strcmp(m_Name,"..");
  Set();
}

cFileListItem::~cFileListItem()
{
  free(m_Name);
}

void cFileListItem::Set(void)
{
  char *txt = NULL,*pt;
  if(m_ShowFlags) {
    if(m_IsDir) {
      asprintf(&txt, "\t\t[%s] ", m_Name); // text2skin requires space at end of string to display item correctly ...
    } else {
      asprintf(&txt, "%c\t%c\t%s", m_HasResume?' ':'*', m_HasSubs ? 'S' : ' ', m_Name);
      if(NULL != (pt = strrchr(txt,'.')))
	*pt = 0;
    }
  } else {
    if(m_IsDir) {
      asprintf(&txt, "[%s] ", m_Name); // text2skin requires space at end of string to display item correctly ...
    } else {
      asprintf(&txt, "%s", m_Name);
      if(NULL != (pt = strrchr(txt,'.')))
	*pt = 0;
    }
  }
  SetText(txt, false);
}

int cFileListItem::Compare(const cListObject &ListObject) const
{
  cFileListItem *other = (cFileListItem *)&ListObject;

  if(m_IsDir && !other->m_IsDir)
    return -1;
  if(!m_IsDir && other->m_IsDir)
    return 1;
  if(m_Up && !other->m_Up)
    return -1;
  if(!m_Up && other->m_Up)
    return 1;
  return strcmp(m_Name,other->m_Name);
}

bool cFileListItem::operator< (const cListObject &ListObject)
{
  cFileListItem *other = (cFileListItem *)&ListObject;

  if(m_IsDir && !other->m_IsDir)
    return true;
  if(!m_IsDir && other->m_IsDir)
    return false;
  if(m_Up && !other->m_Up)
    return true;
  if(!m_Up && other->m_Up)
    return false;
  return strcmp(m_Name,other->m_Name) < 0;
}
