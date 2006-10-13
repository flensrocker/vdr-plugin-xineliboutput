/*
 * display_message.h: Display simple message
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: display_message.h,v 1.1 2006-10-13 04:37:37 phintuka Exp $
 *
 */

#ifndef __DISPLAY_MESSAGE_H
#define __DISPLAY_MESSAGE_H

#include <vdr/osdbase.h>
#include <vdr/skins.h>

class cDisplayMessage : public cOsdObject
{
  cSkinDisplayMessage *displayMessage;
  char *Message;
  int   Timer;
  int   Timeout;

 public:

  cDisplayMessage(const char *message, int timeout = 3)
  {
    displayMessage = NULL;
    Message = strdup(message);
    Timer = 0;
    Timeout = timeout;
  }

  virtual ~cDisplayMessage()
  {
    delete displayMessage;
    free(Message);
  }

  void Update(const char *message)
  {
    Timer = 0;
    free(Message);
    Message = strdup(message);
    Show();
  }

  virtual eOSState ProcessKey(eKeys Key)
  {
    if(Key == kNone && Timer++ > Timeout)
      return osEnd;

    if(Key != kNone) {
LOGMSG("PutBack: %d , %s", (int)Key, cKey::ToString(Key));
      // put back and close
      cRemote::Put(Key, true);
      return osEnd;
    }

    return osContinue;
  }

  virtual void Show(void)
  {
if(cRemote::HasKeys()) LOGMSG("Show, HasKeys");
    if(!displayMessage)
      displayMessage = Skins.Current()->DisplayMessage();

    displayMessage->SetMessage(mtInfo, Message);
    displayMessage->Flush();
  }

};

#endif
