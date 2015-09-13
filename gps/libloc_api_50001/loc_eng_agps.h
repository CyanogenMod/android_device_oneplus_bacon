/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation, nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __LOC_ENG_AGPS_H__
#define __LOC_ENG_AGPS_H__

#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <arpa/inet.h>
#include <hardware/gps.h>
#include <gps_extended.h>
#include <loc_core_log.h>
#include <linked_list.h>
#include <loc_timer.h>
#include <LocEngAdapter.h>

// forward declaration
class AgpsStateMachine;
class Subscriber;

// NIF resource events
typedef enum {
    RSRC_SUBSCRIBE,
    RSRC_UNSUBSCRIBE,
    RSRC_GRANTED,
    RSRC_RELEASED,
    RSRC_DENIED,
    RSRC_STATUS_MAX
} AgpsRsrcStatus;

typedef enum {
    servicerTypeNoCbParam,
    servicerTypeAgps,
    servicerTypeExt
}servicerType;

//DS Callback struct
typedef struct {
    LocEngAdapter *mAdapter;
    AGpsStatusValue action;
}dsCbData;

// information bundle for subscribers
struct Notification {
    // goes to every subscriber
    static const int BROADCAST_ALL;
    // goes to every ACTIVE subscriber
    static const int BROADCAST_ACTIVE;
    // goes to every INACTIVE subscriber
    static const int BROADCAST_INACTIVE;

    // go to a specific subscriber
    const Subscriber* rcver;
    // broadcast
    const int groupID;
    // the new resource status event
    const AgpsRsrcStatus rsrcStatus;
    // should the subscriber be deleted after the notification
    const bool postNotifyDelete;

    // convenient constructor
    inline Notification(const int broadcast,
                        const AgpsRsrcStatus status,
                        const bool deleteAfterwards) :
        rcver(NULL), groupID(broadcast), rsrcStatus(status),
        postNotifyDelete(deleteAfterwards) {}

    // convenient constructor
    inline Notification(const Subscriber* subscriber,
                        const AgpsRsrcStatus status,
                        const bool deleteAfterwards) :
        rcver(subscriber), groupID(-1), rsrcStatus(status),
        postNotifyDelete(deleteAfterwards) {}

    // convenient constructor
    inline Notification(const int broadcast) :
        rcver(NULL), groupID(broadcast), rsrcStatus(RSRC_STATUS_MAX),
        postNotifyDelete(false) {}

    // convenient constructor
    inline Notification(const Subscriber* subscriber) :
        rcver(subscriber), groupID(-1), rsrcStatus(RSRC_STATUS_MAX),
        postNotifyDelete(false) {}
};

class AgpsState {
    // allows AgpsStateMachine to access private data
    // no class members are public.  We don't want
    // anyone but state machine to use state.
    friend class AgpsStateMachine;
    friend class DSStateMachine;
    // state transitions are done here.
    // Each state implements its own transitions (of course).
    inline virtual AgpsState* onRsrcEvent(AgpsRsrcStatus event, void* data) = 0;

protected:
    // handle back to state machine
    const AgpsStateMachine* mStateMachine;
    // each state has pointers to all 3 states
    // one of which is to itself.
    AgpsState* mReleasedState;
    AgpsState* mAcquiredState;
    AgpsState* mPendingState;
    AgpsState* mReleasingState;

    inline AgpsState(const AgpsStateMachine *stateMachine) :
        mStateMachine(stateMachine),
        mReleasedState(NULL),
        mAcquiredState(NULL),
        mPendingState(NULL),
        mReleasingState(NULL) {}
    virtual ~AgpsState() {}

public:
    // for logging purpose
    inline virtual char* whoami() = 0;
};

class Servicer {
    void (*callback)(void);
public:
    static Servicer* getServicer(servicerType type, void *cb_func);
    virtual int requestRsrc(void *cb_data);
    Servicer() {}
    Servicer(void *cb_func)
    { callback = (void(*)(void))(cb_func); }
    virtual ~Servicer(){}
    inline virtual char *whoami() {return (char*)"Servicer";}
};

class ExtServicer : public Servicer {
    int (*callbackExt)(void *cb_data);
public:
    int requestRsrc(void *cb_data);
    ExtServicer() {}
    ExtServicer(void *cb_func)
    { callbackExt = (int(*)(void *))(cb_func); }
    virtual ~ExtServicer(){}
    inline virtual char *whoami() {return (char*)"ExtServicer";}
};

class AGpsServicer : public Servicer {
    void (*callbackAGps)(AGpsStatus* status);
public:
    int requestRsrc(void *cb_data);
    AGpsServicer() {}
    AGpsServicer(void *cb_func)
    { callbackAGps = (void(*)(AGpsStatus *))(cb_func); }
    virtual ~AGpsServicer(){}
    inline virtual char *whoami() {return (char*)"AGpsServicer";}
};

class AgpsStateMachine {
protected:
    // a linked list of subscribers.
    void* mSubscribers;
    //handle to whoever provides the service
    Servicer *mServicer;
    // allows AgpsState to access private data
    // each state is really internal data to the
    // state machine, so it should be able to
    // access anything within the state machine.
    friend class AgpsState;
    // pointer to the current state.
    AgpsState* mStatePtr;
private:
    // NIF type: AGNSS or INTERNET.
    const AGpsExtType mType;
    // apn to the NIF.  Each state machine tracks
    // resource state of a particular NIF.  For each
    // NIF, there is also an active APN.
    char* mAPN;
    // for convenience, we don't do strlen each time.
    unsigned int mAPNLen;
    // bear
    AGpsBearerType mBearer;
    // ipv4 address for routing
    bool mEnforceSingleSubscriber;

public:
    AgpsStateMachine(servicerType servType, void *cb_func,
                     AGpsExtType type, bool enforceSingleSubscriber);
    virtual ~AgpsStateMachine();

    // self explanatory methods below
    void setAPN(const char* apn, unsigned int len);
    inline const char* getAPN() const { return (const char*)mAPN; }
    inline void setBearer(AGpsBearerType bearer) { mBearer = bearer; }
    inline AGpsBearerType getBearer() const { return mBearer; }
    inline AGpsExtType getType() const { return (AGpsExtType)mType; }

    // someone, a ATL client or BIT, is asking for NIF
    void subscribeRsrc(Subscriber *subscriber);

    // someone, a ATL client or BIT, is done with NIF
    bool unsubscribeRsrc(Subscriber *subscriber);

    // add a subscriber in the linked list, if not already there.
    void addSubscriber(Subscriber* subscriber) const;

    virtual void onRsrcEvent(AgpsRsrcStatus event);

    // put the data together and send the FW
    virtual int sendRsrcRequest(AGpsStatusValue action) const;

    //if list is empty, linked_list_empty returns 1
    //else if list is not empty, returns 0
    //so hasSubscribers() returns 1 if list is not empty
    //and returns 0 if list is empty
    inline bool hasSubscribers() const
    { return !linked_list_empty(mSubscribers); }

    bool hasActiveSubscribers() const;

    inline void dropAllSubscribers() const
    { linked_list_flush(mSubscribers); }

    // private. Only a state gets to call this.
    void notifySubscribers(Notification& notification) const;

};

class DSStateMachine : public AgpsStateMachine {
    static const unsigned char MAX_START_DATA_CALL_RETRIES;
    static const unsigned int DATA_CALL_RETRY_DELAY_MSEC;
    LocEngAdapter* mLocAdapter;
    unsigned char mRetries;
public:
    DSStateMachine(servicerType type,
                   void *cb_func,
                   LocEngAdapter* adapterHandle);
    int sendRsrcRequest(AGpsStatusValue action) const;
    void onRsrcEvent(AgpsRsrcStatus event);
    void retryCallback();
    void informStatus(AgpsRsrcStatus status, int ID) const;
    inline void incRetries() {mRetries++;}
    inline virtual char *whoami() {return (char*)"DSStateMachine";}
};

// each subscriber is a AGPS client.  In the case of ATL, there could be
// multiple clients from modem.  In the case of BIT, there is only one
// cilent from BIT daemon.
struct Subscriber {
    const uint32_t ID;
    const AgpsStateMachine* mStateMachine;
    inline Subscriber(const int id,
                      const AgpsStateMachine* stateMachine) :
        ID(id), mStateMachine(stateMachine) {}
    inline virtual ~Subscriber() {}

    virtual void setIPAddresses(uint32_t &v4, char* v6) = 0;
    inline virtual void setWifiInfo(char* ssid, char* password)
    { ssid[0] = 0; password[0] = 0; }

    inline virtual bool equals(const Subscriber *s) const
    { return ID == s->ID; }

    // notifies a subscriber a new NIF resource status, usually
    // either GRANTE, DENIED, or RELEASED
    virtual bool notifyRsrcStatus(Notification &notification) = 0;

    virtual bool waitForCloseComplete() { return false; }
    virtual void setInactive() {}
    virtual bool isInactive() { return false; }

    virtual Subscriber* clone() = 0;
    // checks if this notification is for me, i.e.
    // either has my id, or has a broadcast id.
    bool forMe(Notification &notification);
};

// BITSubscriber, created with requests from BIT daemon
struct BITSubscriber : public Subscriber {
    char mIPv6Addr[16];

    inline BITSubscriber(const AgpsStateMachine* stateMachine,
                         unsigned int ipv4, char* ipv6) :
        Subscriber(ipv4, stateMachine)
    {
        if (NULL == ipv6) {
            mIPv6Addr[0] = 0;
        } else {
            memcpy(mIPv6Addr, ipv6, sizeof(mIPv6Addr));
        }
    }

    virtual bool notifyRsrcStatus(Notification &notification);

    inline virtual void setIPAddresses(uint32_t &v4, char* v6)
    { v4 = ID; memcpy(v6, mIPv6Addr, sizeof(mIPv6Addr)); }

    virtual Subscriber* clone()
    {
        return new BITSubscriber(mStateMachine, ID, mIPv6Addr);
    }

    virtual bool equals(const Subscriber *s) const;
    inline virtual ~BITSubscriber(){}
};

// ATLSubscriber, created with requests from ATL
struct ATLSubscriber : public Subscriber {
    const LocEngAdapter* mLocAdapter;
    const bool mBackwardCompatibleMode;
    inline ATLSubscriber(const int id,
                         const AgpsStateMachine* stateMachine,
                         const LocEngAdapter* adapter,
                         const bool compatibleMode) :
        Subscriber(id, stateMachine), mLocAdapter(adapter),
        mBackwardCompatibleMode(compatibleMode){}
    virtual bool notifyRsrcStatus(Notification &notification);

    inline virtual void setIPAddresses(uint32_t &v4, char* v6)
    { v4 = INADDR_NONE; v6[0] = 0; }

    inline virtual Subscriber* clone()
    {
        return new ATLSubscriber(ID, mStateMachine, mLocAdapter,
                                 mBackwardCompatibleMode);
    }
    inline virtual ~ATLSubscriber(){}
};

// WIFISubscriber, created with requests from MSAPM or QuIPC
struct WIFISubscriber : public Subscriber {
    char * mSSID;
    char * mPassword;
    loc_if_req_sender_id_e_type senderId;
    bool mIsInactive;
    inline WIFISubscriber(const AgpsStateMachine* stateMachine,
                         char * ssid, char * password, loc_if_req_sender_id_e_type sender_id) :
        Subscriber(sender_id, stateMachine),
        mSSID(NULL == ssid ? NULL : new char[SSID_BUF_SIZE]),
        mPassword(NULL == password ? NULL : new char[SSID_BUF_SIZE]),
        senderId(sender_id)
    {
      if (NULL != mSSID)
          strlcpy(mSSID, ssid, SSID_BUF_SIZE);
      if (NULL != mPassword)
          strlcpy(mPassword, password, SSID_BUF_SIZE);
      mIsInactive = false;
    }

    virtual bool notifyRsrcStatus(Notification &notification);

    inline virtual void setIPAddresses(uint32_t &v4, char* v6) {}

    inline virtual void setWifiInfo(char* ssid, char* password)
    {
      if (NULL != mSSID)
          strlcpy(ssid, mSSID, SSID_BUF_SIZE);
      else
          ssid[0] = '\0';
      if (NULL != mPassword)
          strlcpy(password, mPassword, SSID_BUF_SIZE);
      else
          password[0] = '\0';
    }

    inline virtual bool waitForCloseComplete() { return true; }

    inline virtual void setInactive() { mIsInactive = true; }
    inline virtual bool isInactive() { return mIsInactive; }

    virtual Subscriber* clone()
    {
        return new WIFISubscriber(mStateMachine, mSSID, mPassword, senderId);
    }
    inline virtual ~WIFISubscriber(){}
};

struct DSSubscriber : public Subscriber {
    bool mIsInactive;
    inline DSSubscriber(const AgpsStateMachine *stateMachine,
                         const int id) :
        Subscriber(id, stateMachine)
    {
        mIsInactive = false;
    }
    inline virtual void setIPAddresses(uint32_t &v4, char* v6) {}
    virtual Subscriber* clone()
    {return new DSSubscriber(mStateMachine, ID);}
    virtual bool notifyRsrcStatus(Notification &notification);
    inline virtual bool waitForCloseComplete() { return true; }
    virtual void setInactive();
    inline virtual bool isInactive()
    { return mIsInactive; }
    inline virtual ~DSSubscriber(){}
    inline virtual char *whoami() {return (char*)"DSSubscriber";}
};

#endif //__LOC_ENG_AGPS_H__
