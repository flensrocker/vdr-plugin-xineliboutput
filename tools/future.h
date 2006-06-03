/*
 * future.h: A variable that gets its value in future.
 *           Used to convert asynchronous IPCs to synchronous.
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: future.h,v 1.1 2006-06-03 10:04:27 phintuka Exp $
 *
 */

#ifndef __FUTURE_H
#define __FUTURE_H

#include <vdr/thread.h>

template <class T>
class cFuture {

  private:
    cMutex mutex;
    cCondVar cond;
    bool m_Ready;
    T m_Value;

  public:

    cFuture() 
    {
      m_Ready = false;
    }

    void Reset(void)
    {
      cMutexLock l(&mutex);
      m_Ready = false;
    }

    //
    // Producer interface
    //

    void Set(T& Value)
    {
      cMutexLock l(&mutex);
      m_Value = Value;
      m_Ready = true;
      cond.Broadcast();
    }

    //
    // Consumer interface
    //

    bool Wait(int Timeout = -1)
    {
      cMutexLock l(&mutex);

      if(Timeout >= 0)
	return cond.TimedWait(mutex, Timeout);

      while(!m_Ready)
	cond.Wait(mutex);

      return true;
    }

    bool IsReady(void)
    {
      cMutexLock l(&mutex);
      return m_Ready;
    }

    T Value(void)
    {
      cMutexLock l(&mutex);
      while(!m_Ready)
	cond.Wait(mutex);
      return m_Value;
    }
};


#endif // __FUTURE_H
