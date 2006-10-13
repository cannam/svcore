/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/


/*
   This is a modified version of a source file from the 
   Rosegarden MIDI and audio sequencer and notation editor.
   This file copyright 2000-2006 Richard Bown and Chris Cannam.
*/


#include <iostream>
#include <fstream>
#include <string>
#include <cstdio>
#include <algorithm>

#include "MIDIFileReader.h"

#include "model/Model.h"
#include "base/Pitch.h"
#include "base/RealTime.h"
#include "model/NoteModel.h"

#include <QString>
#include <QMessageBox>
#include <QInputDialog>

#include <sstream>

using std::string;
using std::ifstream;
using std::stringstream;
using std::cerr;
using std::endl;
using std::ends;
using std::ios;
using std::vector;
using std::map;
using std::set;

//#define MIDI_DEBUG 1

static const char *const MIDI_FILE_HEADER         = "MThd";
static const char *const MIDI_TRACK_HEADER        = "MTrk";

static const MIDIFileReader::MIDIByte MIDI_STATUS_BYTE_MASK       = 0x80;
static const MIDIFileReader::MIDIByte MIDI_MESSAGE_TYPE_MASK      = 0xF0;
static const MIDIFileReader::MIDIByte MIDI_CHANNEL_NUM_MASK       = 0x0F;
static const MIDIFileReader::MIDIByte MIDI_NOTE_OFF               = 0x80;
static const MIDIFileReader::MIDIByte MIDI_NOTE_ON                = 0x90;
static const MIDIFileReader::MIDIByte MIDI_POLY_AFTERTOUCH        = 0xA0;
static const MIDIFileReader::MIDIByte MIDI_CTRL_CHANGE            = 0xB0;
static const MIDIFileReader::MIDIByte MIDI_PROG_CHANGE            = 0xC0;
static const MIDIFileReader::MIDIByte MIDI_CHNL_AFTERTOUCH        = 0xD0;
static const MIDIFileReader::MIDIByte MIDI_PITCH_BEND             = 0xE0;
static const MIDIFileReader::MIDIByte MIDI_SELECT_CHNL_MODE       = 0xB0;
static const MIDIFileReader::MIDIByte MIDI_SYSTEM_EXCLUSIVE       = 0xF0;
static const MIDIFileReader::MIDIByte MIDI_TC_QUARTER_FRAME       = 0xF1;
static const MIDIFileReader::MIDIByte MIDI_SONG_POSITION_PTR      = 0xF2;
static const MIDIFileReader::MIDIByte MIDI_SONG_SELECT            = 0xF3;
static const MIDIFileReader::MIDIByte MIDI_TUNE_REQUEST           = 0xF6;
static const MIDIFileReader::MIDIByte MIDI_END_OF_EXCLUSIVE       = 0xF7;
static const MIDIFileReader::MIDIByte MIDI_TIMING_CLOCK           = 0xF8;
static const MIDIFileReader::MIDIByte MIDI_START                  = 0xFA;
static const MIDIFileReader::MIDIByte MIDI_CONTINUE               = 0xFB;
static const MIDIFileReader::MIDIByte MIDI_STOP                   = 0xFC;
static const MIDIFileReader::MIDIByte MIDI_ACTIVE_SENSING         = 0xFE;
static const MIDIFileReader::MIDIByte MIDI_SYSTEM_RESET           = 0xFF;
static const MIDIFileReader::MIDIByte MIDI_SYSEX_NONCOMMERCIAL    = 0x7D;
static const MIDIFileReader::MIDIByte MIDI_SYSEX_NON_RT           = 0x7E;
static const MIDIFileReader::MIDIByte MIDI_SYSEX_RT               = 0x7F;
static const MIDIFileReader::MIDIByte MIDI_SYSEX_RT_COMMAND       = 0x06;
static const MIDIFileReader::MIDIByte MIDI_SYSEX_RT_RESPONSE      = 0x07;
static const MIDIFileReader::MIDIByte MIDI_MMC_STOP               = 0x01;
static const MIDIFileReader::MIDIByte MIDI_MMC_PLAY               = 0x02;
static const MIDIFileReader::MIDIByte MIDI_MMC_DEFERRED_PLAY      = 0x03;
static const MIDIFileReader::MIDIByte MIDI_MMC_FAST_FORWARD       = 0x04;
static const MIDIFileReader::MIDIByte MIDI_MMC_REWIND             = 0x05;
static const MIDIFileReader::MIDIByte MIDI_MMC_RECORD_STROBE      = 0x06;
static const MIDIFileReader::MIDIByte MIDI_MMC_RECORD_EXIT        = 0x07;
static const MIDIFileReader::MIDIByte MIDI_MMC_RECORD_PAUSE       = 0x08;
static const MIDIFileReader::MIDIByte MIDI_MMC_PAUSE              = 0x08;
static const MIDIFileReader::MIDIByte MIDI_MMC_EJECT              = 0x0A;
static const MIDIFileReader::MIDIByte MIDI_MMC_LOCATE             = 0x44;
static const MIDIFileReader::MIDIByte MIDI_FILE_META_EVENT        = 0xFF;
static const MIDIFileReader::MIDIByte MIDI_SEQUENCE_NUMBER        = 0x00;
static const MIDIFileReader::MIDIByte MIDI_TEXT_EVENT             = 0x01;
static const MIDIFileReader::MIDIByte MIDI_COPYRIGHT_NOTICE       = 0x02;
static const MIDIFileReader::MIDIByte MIDI_TRACK_NAME             = 0x03;
static const MIDIFileReader::MIDIByte MIDI_INSTRUMENT_NAME        = 0x04;
static const MIDIFileReader::MIDIByte MIDI_LYRIC                  = 0x05;
static const MIDIFileReader::MIDIByte MIDI_TEXT_MARKER            = 0x06;
static const MIDIFileReader::MIDIByte MIDI_CUE_POINT              = 0x07;
static const MIDIFileReader::MIDIByte MIDI_CHANNEL_PREFIX         = 0x20;
static const MIDIFileReader::MIDIByte MIDI_CHANNEL_PREFIX_OR_PORT = 0x21;
static const MIDIFileReader::MIDIByte MIDI_END_OF_TRACK           = 0x2F;
static const MIDIFileReader::MIDIByte MIDI_SET_TEMPO              = 0x51;
static const MIDIFileReader::MIDIByte MIDI_SMPTE_OFFSET           = 0x54;
static const MIDIFileReader::MIDIByte MIDI_TIME_SIGNATURE         = 0x58;
static const MIDIFileReader::MIDIByte MIDI_KEY_SIGNATURE          = 0x59;
static const MIDIFileReader::MIDIByte MIDI_SEQUENCER_SPECIFIC     = 0x7F;
static const MIDIFileReader::MIDIByte MIDI_CONTROLLER_BANK_MSB      = 0x00;
static const MIDIFileReader::MIDIByte MIDI_CONTROLLER_VOLUME        = 0x07;
static const MIDIFileReader::MIDIByte MIDI_CONTROLLER_BANK_LSB      = 0x20;
static const MIDIFileReader::MIDIByte MIDI_CONTROLLER_MODULATION    = 0x01;
static const MIDIFileReader::MIDIByte MIDI_CONTROLLER_PAN           = 0x0A;
static const MIDIFileReader::MIDIByte MIDI_CONTROLLER_SUSTAIN       = 0x40;
static const MIDIFileReader::MIDIByte MIDI_CONTROLLER_RESONANCE     = 0x47;
static const MIDIFileReader::MIDIByte MIDI_CONTROLLER_RELEASE       = 0x48;
static const MIDIFileReader::MIDIByte MIDI_CONTROLLER_ATTACK        = 0x49;
static const MIDIFileReader::MIDIByte MIDI_CONTROLLER_FILTER        = 0x4A;
static const MIDIFileReader::MIDIByte MIDI_CONTROLLER_REVERB        = 0x5B;
static const MIDIFileReader::MIDIByte MIDI_CONTROLLER_CHORUS        = 0x5D;
static const MIDIFileReader::MIDIByte MIDI_CONTROLLER_NRPN_1        = 0x62;
static const MIDIFileReader::MIDIByte MIDI_CONTROLLER_NRPN_2        = 0x63;
static const MIDIFileReader::MIDIByte MIDI_CONTROLLER_RPN_1         = 0x64;
static const MIDIFileReader::MIDIByte MIDI_CONTROLLER_RPN_2         = 0x65;
static const MIDIFileReader::MIDIByte MIDI_CONTROLLER_SOUNDS_OFF    = 0x78;
static const MIDIFileReader::MIDIByte MIDI_CONTROLLER_RESET         = 0x79;
static const MIDIFileReader::MIDIByte MIDI_CONTROLLER_LOCAL         = 0x7A;
static const MIDIFileReader::MIDIByte MIDI_CONTROLLER_ALL_NOTES_OFF = 0x7B;
static const MIDIFileReader::MIDIByte MIDI_PERCUSSION_CHANNEL       = 9;

class MIDIEvent
{
public:
    typedef MIDIFileReader::MIDIByte MIDIByte;

    MIDIEvent(unsigned long deltaTime,
              MIDIByte eventCode,
              MIDIByte data1 = 0,
              MIDIByte data2 = 0) :
	m_deltaTime(deltaTime),
	m_duration(0),
	m_eventCode(eventCode),
	m_data1(data1),
	m_data2(data2),
	m_metaEventCode(0)
    { }

    MIDIEvent(unsigned long deltaTime,
              MIDIByte eventCode,
              MIDIByte metaEventCode,
              const string &metaMessage) :
	m_deltaTime(deltaTime),
	m_duration(0),
	m_eventCode(eventCode),
	m_data1(0),
	m_data2(0),
	m_metaEventCode(metaEventCode),
	m_metaMessage(metaMessage)
    { }

    MIDIEvent(unsigned long deltaTime,
              MIDIByte eventCode,
              const string &sysEx) :
	m_deltaTime(deltaTime),
	m_duration(0),
	m_eventCode(eventCode),
	m_data1(0),
	m_data2(0),
	m_metaEventCode(0),
	m_metaMessage(sysEx)
    { }

    ~MIDIEvent() { }

    void setTime(const unsigned long &time) { m_deltaTime = time; }
    void setDuration(const unsigned long& duration) { m_duration = duration;}
    unsigned long addTime(const unsigned long &time) {
	m_deltaTime += time;
	return m_deltaTime;
    }

    MIDIByte getMessageType() const
        { return (m_eventCode & MIDI_MESSAGE_TYPE_MASK); }

    MIDIByte getChannelNumber() const
        { return (m_eventCode & MIDI_CHANNEL_NUM_MASK); }

    unsigned long getTime() const { return m_deltaTime; }
    unsigned long getDuration() const { return m_duration; }

    MIDIByte getPitch() const { return m_data1; }
    MIDIByte getVelocity() const { return m_data2; }
    MIDIByte getData1() const { return m_data1; }
    MIDIByte getData2() const { return m_data2; }
    MIDIByte getEventCode() const { return m_eventCode; }

    bool isMeta() const { return (m_eventCode == MIDI_FILE_META_EVENT); }

    MIDIByte getMetaEventCode() const { return m_metaEventCode; }
    string getMetaMessage() const { return m_metaMessage; }
    void setMetaMessage(const string &meta) { m_metaMessage = meta; }

    friend bool operator<(const MIDIEvent &a, const MIDIEvent &b);

private:
    MIDIEvent& operator=(const MIDIEvent);

    unsigned long  m_deltaTime;
    unsigned long  m_duration;
    MIDIByte       m_eventCode;
    MIDIByte       m_data1;         // or Note
    MIDIByte       m_data2;         // or Velocity
    MIDIByte       m_metaEventCode;
    string         m_metaMessage;
};

// Comparator for sorting
//
struct MIDIEventCmp
{
    bool operator()(const MIDIEvent &mE1, const MIDIEvent &mE2) const
    { return mE1.getTime() < mE2.getTime(); }

    bool operator()(const MIDIEvent *mE1, const MIDIEvent *mE2) const
    { return mE1->getTime() < mE2->getTime(); }
};

class MIDIException : virtual public std::exception
{
public:
    MIDIException(QString message) throw() : m_message(message) {
	cerr << "WARNING: MIDI exception: "
		  << message.toLocal8Bit().data() << endl;
    }
    virtual ~MIDIException() throw() { }

    virtual const char *what() const throw() {
	return m_message.toLocal8Bit().data();
    }

protected:
    QString m_message;
};


MIDIFileReader::MIDIFileReader(QString path,
			       size_t mainModelSampleRate) :
    m_timingDivision(0),
    m_format(MIDI_FILE_BAD_FORMAT),
    m_numberOfTracks(0),
    m_trackByteCount(0),
    m_decrementCount(false),
    m_path(path),
    m_midiFile(0),
    m_fileSize(0),
    m_mainModelSampleRate(mainModelSampleRate)
{
    if (parseFile()) {
	m_error = "";
    }
}

MIDIFileReader::~MIDIFileReader()
{
    for (MIDIComposition::iterator i = m_midiComposition.begin();
	 i != m_midiComposition.end(); ++i) {
	
	for (MIDITrack::iterator j = i->second.begin();
	     j != i->second.end(); ++j) {
	    delete *j;
	}

	i->second.clear();
    }

    m_midiComposition.clear();
}

bool
MIDIFileReader::isOK() const
{
    return (m_error == "");
}

QString
MIDIFileReader::getError() const
{
    return m_error;
}

long
MIDIFileReader::midiBytesToLong(const string& bytes)
{
    if (bytes.length() != 4) {
	throw MIDIException(tr("Wrong length for long data in MIDI stream (%1, should be %2)").arg(bytes.length()).arg(4));
    }

    long longRet = ((long)(((MIDIByte)bytes[0]) << 24)) |
                   ((long)(((MIDIByte)bytes[1]) << 16)) |
                   ((long)(((MIDIByte)bytes[2]) << 8)) |
                   ((long)((MIDIByte)(bytes[3])));

    return longRet;
}

int
MIDIFileReader::midiBytesToInt(const string& bytes)
{
    if (bytes.length() != 2) {
	throw MIDIException(tr("Wrong length for int data in MIDI stream (%1, should be %2)").arg(bytes.length()).arg(2));
    }

    int intRet = ((int)(((MIDIByte)bytes[0]) << 8)) |
                 ((int)(((MIDIByte)bytes[1])));
    return(intRet);
}


// Gets a single byte from the MIDI byte stream.  For each track
// section we can read only a specified number of bytes held in
// m_trackByteCount.
//
MIDIFileReader::MIDIByte
MIDIFileReader::getMIDIByte()
{
    if (!m_midiFile) {
	throw MIDIException(tr("getMIDIByte called but no MIDI file open"));
    }

    if (m_midiFile->eof()) {
        throw MIDIException(tr("End of MIDI file encountered while reading"));
    }

    if (m_decrementCount && m_trackByteCount <= 0) {
        throw MIDIException(tr("Attempt to get more bytes than expected on Track"));
    }

    char byte;
    if (m_midiFile->read(&byte, 1)) {
	--m_trackByteCount;
	return (MIDIByte)byte;
    }

    throw MIDIException(tr("Attempt to read past MIDI file end"));
}


// Gets a specified number of bytes from the MIDI byte stream.  For
// each track section we can read only a specified number of bytes
// held in m_trackByteCount.
//
string
MIDIFileReader::getMIDIBytes(unsigned long numberOfBytes)
{
    if (!m_midiFile) {
	throw MIDIException(tr("getMIDIBytes called but no MIDI file open"));
    }

    if (m_midiFile->eof()) {
        throw MIDIException(tr("End of MIDI file encountered while reading"));
    }

    if (m_decrementCount && (numberOfBytes > (unsigned long)m_trackByteCount)) {
        throw MIDIException(tr("Attempt to get more bytes than available on Track (%1, only have %2)").arg(numberOfBytes).arg(m_trackByteCount));
    }

    string stringRet;
    char fileMIDIByte;

    while (stringRet.length() < numberOfBytes &&
           m_midiFile->read(&fileMIDIByte, 1)) {
        stringRet += fileMIDIByte;
    }

    // if we've reached the end of file without fulfilling the
    // quota then panic as our parsing has performed incorrectly
    //
    if (stringRet.length() < numberOfBytes) {
        stringRet = "";
        throw MIDIException(tr("Attempt to read past MIDI file end"));
    }

    // decrement the byte count
    if (m_decrementCount)
        m_trackByteCount -= stringRet.length();

    return stringRet;
}


// Get a long number of variable length from the MIDI byte stream.
//
long
MIDIFileReader::getNumberFromMIDIBytes(int firstByte)
{
    if (!m_midiFile) {
	throw MIDIException(tr("getNumberFromMIDIBytes called but no MIDI file open"));
    }

    long longRet = 0;
    MIDIByte midiByte;

    if (firstByte >= 0) {
	midiByte = (MIDIByte)firstByte;
    } else if (m_midiFile->eof()) {
	return longRet;
    } else {
	midiByte = getMIDIByte();
    }

    longRet = midiByte;
    if (midiByte & 0x80) {
	longRet &= 0x7F;
	do {
	    midiByte = getMIDIByte();
	    longRet = (longRet << 7) + (midiByte & 0x7F);
	} while (!m_midiFile->eof() && (midiByte & 0x80));
    }

    return longRet;
}


// Seek to the next track in the midi file and set the number
// of bytes to be read in the counter m_trackByteCount.
//
bool
MIDIFileReader::skipToNextTrack()
{
    if (!m_midiFile) {
	throw MIDIException(tr("skipToNextTrack called but no MIDI file open"));
    }

    string buffer, buffer2;
    m_trackByteCount = -1;
    m_decrementCount = false;

    while (!m_midiFile->eof() && (m_decrementCount == false)) {
        buffer = getMIDIBytes(4); 
	if (buffer.compare(0, 4, MIDI_TRACK_HEADER) == 0) {
	    m_trackByteCount = midiBytesToLong(getMIDIBytes(4));
	    m_decrementCount = true;
	}
    }

    if (m_trackByteCount == -1) { // we haven't found a track
        return false;
    } else {
        return true;
    }
}


// Read in a MIDI file.  The parsing process throws exceptions back up
// here if we run into trouble which we can then pass back out to
// whoever called us using a nice bool.
//
bool
MIDIFileReader::parseFile()
{
    m_error = "";

#ifdef MIDI_DEBUG
    cerr << "MIDIFileReader::open() : fileName = " << m_fileName.c_str() << endl;
#endif

    // Open the file
    m_midiFile = new ifstream(m_path.toLocal8Bit().data(),
			      ios::in | ios::binary);

    if (!*m_midiFile) {
	m_error = "File not found or not readable.";
	m_format = MIDI_FILE_BAD_FORMAT;
	delete m_midiFile;
	return false;
    }

    bool retval = false;

    try {

	// Set file size so we can count it off
	//
	m_midiFile->seekg(0, ios::end);
	m_fileSize = m_midiFile->tellg();
	m_midiFile->seekg(0, ios::beg);

	// Parse the MIDI header first.  The first 14 bytes of the file.
	if (!parseHeader(getMIDIBytes(14))) {
	    m_format = MIDI_FILE_BAD_FORMAT;
	    m_error = "Not a MIDI file.";
	    goto done;
	}

	unsigned int i = 0;

	for (unsigned int j = 0; j < m_numberOfTracks; ++j) {

#ifdef MIDI_DEBUG
	    cerr << "Parsing Track " << j << endl;
#endif

	    if (!skipToNextTrack()) {
#ifdef MIDI_DEBUG
		cerr << "Couldn't find Track " << j << endl;
#endif
		m_error = "File corrupted or in non-standard format?";
		m_format = MIDI_FILE_BAD_FORMAT;
		goto done;
	    }

#ifdef MIDI_DEBUG
	    cerr << "Track has " << m_trackByteCount << " bytes" << endl;
#endif

	    // Run through the events taking them into our internal
	    // representation.
	    if (!parseTrack(i)) {
#ifdef MIDI_DEBUG
		cerr << "Track " << j << " parsing failed" << endl;
#endif
		m_error = "File corrupted or in non-standard format?";
		m_format = MIDI_FILE_BAD_FORMAT;
		goto done;
	    }

	    ++i; // j is the source track number, i the destination
	}
	
	m_numberOfTracks = i;
	retval = true;

    } catch (MIDIException e) {

        cerr << "MIDIFileReader::open() - caught exception - " << e.what() << endl;
	m_error = e.what();
    }
    
done:
    m_midiFile->close();
    delete m_midiFile;

    for (unsigned int track = 0; track < m_numberOfTracks; ++track) {

        // Convert the deltaTime to an absolute time since the track
        // start.  The addTime method returns the sum of the current
        // MIDI Event delta time plus the argument.

	unsigned long acc = 0;

        for (MIDITrack::iterator i = m_midiComposition[track].begin();
             i != m_midiComposition[track].end(); ++i) {
            acc = (*i)->addTime(acc);
        }

        if (consolidateNoteOffEvents(track)) { // returns true if some notes exist
	    m_loadableTracks.insert(track);
	}
    }

    for (unsigned int track = 0; track < m_numberOfTracks; ++track) {
        updateTempoMap(track);
    }

    calculateTempoTimestamps();

    return retval;
}

// Parse and ensure the MIDI Header is legitimate
//
bool
MIDIFileReader::parseHeader(const string &midiHeader)
{
    if (midiHeader.size() < 14) {
#ifdef MIDI_DEBUG
        cerr << "MIDIFileReader::parseHeader() - file header undersized" << endl;
#endif
        return false;
    }

    if (midiHeader.compare(0, 4, MIDI_FILE_HEADER) != 0) {
#ifdef MIDI_DEBUG
	cerr << "MIDIFileReader::parseHeader()"
	     << "- file header not found or malformed"
	     << endl;
#endif
	return false;
    }

    if (midiBytesToLong(midiHeader.substr(4,4)) != 6L) {
#ifdef MIDI_DEBUG
        cerr << "MIDIFileReader::parseHeader()"
	     << " - header length incorrect"
	     << endl;
#endif
        return false;
    }

    m_format = (MIDIFileFormatType) midiBytesToInt(midiHeader.substr(8,2));
    m_numberOfTracks = midiBytesToInt(midiHeader.substr(10,2));
    m_timingDivision = midiBytesToInt(midiHeader.substr(12,2));

    if (m_format == MIDI_SEQUENTIAL_TRACK_FILE) {
#ifdef MIDI_DEBUG
        cerr << "MIDIFileReader::parseHeader()"
                  << "- can't load sequential track file"
                  << endl;
#endif
        return false;
    }

#ifdef MIDI_DEBUG
    if (m_timingDivision < 0) {
        cerr << "MIDIFileReader::parseHeader()"
                  << " - file uses SMPTE timing"
                  << endl;
    }
#endif

    return true; 
}

// Extract the contents from a MIDI file track and places it into
// our local map of MIDI events.
//
bool
MIDIFileReader::parseTrack(unsigned int &lastTrackNum)
{
    MIDIByte midiByte, metaEventCode, data1, data2;
    MIDIByte eventCode = 0x80;
    string metaMessage;
    unsigned int messageLength;
    unsigned long deltaTime;
    unsigned long accumulatedTime = 0;

    // The trackNum passed in to this method is the default track for
    // all events provided they're all on the same channel.  If we find
    // events on more than one channel, we increment trackNum and record
    // the mapping from channel to trackNum in this channelTrackMap.
    // We then return the new trackNum by reference so the calling
    // method knows we've got more tracks than expected.

    // This would be a vector<unsigned int> but we need -1 to indicate
    // "not yet used"
    vector<int> channelTrackMap(16, -1);

    // This is used to store the last absolute time found on each track,
    // allowing us to modify delta-times correctly when separating events
    // out from one to multiple tracks
    //
    map<int, unsigned long> trackTimeMap;

    // Meta-events don't have a channel, so we place them in a fixed
    // track number instead
    unsigned int metaTrack = lastTrackNum;

    // Remember the last non-meta status byte (-1 if we haven't seen one)
    int runningStatus = -1;

    bool firstTrack = true;

    while (!m_midiFile->eof() && (m_trackByteCount > 0)) {

	if (eventCode < 0x80) {
#ifdef MIDI_DEBUG
	    cerr << "WARNING: Invalid event code " << eventCode
		 << " in MIDI file" << endl;
#endif
	    throw MIDIException(tr("Invalid event code %1 found").arg(int(eventCode)));
	}

        deltaTime = getNumberFromMIDIBytes();

#ifdef MIDI_DEBUG
	cerr << "read delta time " << deltaTime << endl;
#endif

        // Get a single byte
        midiByte = getMIDIByte();

        if (!(midiByte & MIDI_STATUS_BYTE_MASK)) {

	    if (runningStatus < 0) {
		throw MIDIException(tr("Running status used for first event in track"));
	    }

	    eventCode = (MIDIByte)runningStatus;
	    data1 = midiByte;

#ifdef MIDI_DEBUG
	    cerr << "using running status (byte " << int(midiByte) << " found)" << endl;
#endif
        } else {
#ifdef MIDI_DEBUG
	    cerr << "have new event code " << int(midiByte) << endl;
#endif
            eventCode = midiByte;
	    data1 = getMIDIByte();
	}

        if (eventCode == MIDI_FILE_META_EVENT) {

	    metaEventCode = data1;
            messageLength = getNumberFromMIDIBytes();

//#ifdef MIDI_DEBUG
		cerr << "Meta event of type " << int(metaEventCode) << " and " << messageLength << " bytes found, putting on track " << metaTrack << endl;
//#endif
            metaMessage = getMIDIBytes(messageLength);

	    long gap = accumulatedTime - trackTimeMap[metaTrack];
	    accumulatedTime += deltaTime;
	    deltaTime += gap;
	    trackTimeMap[metaTrack] = accumulatedTime;

            MIDIEvent *e = new MIDIEvent(deltaTime,
                                         MIDI_FILE_META_EVENT,
                                         metaEventCode,
                                         metaMessage);

	    m_midiComposition[metaTrack].push_back(e);

	    if (metaEventCode == MIDI_TRACK_NAME) {
		m_trackNames[metaTrack] = metaMessage.c_str();
	    }

        } else { // non-meta events

	    runningStatus = eventCode;

            MIDIEvent *midiEvent;

	    int channel = (eventCode & MIDI_CHANNEL_NUM_MASK);
	    if (channelTrackMap[channel] == -1) {
		if (!firstTrack) ++lastTrackNum;
		else firstTrack = false;
		channelTrackMap[channel] = lastTrackNum;
	    }

	    unsigned int trackNum = channelTrackMap[channel];
	    
	    // accumulatedTime is abs time of last event on any track;
	    // trackTimeMap[trackNum] is that of last event on this track
	    
	    long gap = accumulatedTime - trackTimeMap[trackNum];
	    accumulatedTime += deltaTime;
	    deltaTime += gap;
	    trackTimeMap[trackNum] = accumulatedTime;

            switch (eventCode & MIDI_MESSAGE_TYPE_MASK) {

            case MIDI_NOTE_ON:
            case MIDI_NOTE_OFF:
            case MIDI_POLY_AFTERTOUCH:
            case MIDI_CTRL_CHANGE:
                data2 = getMIDIByte();

                // create and store our event
                midiEvent = new MIDIEvent(deltaTime, eventCode, data1, data2);

                /*
		cerr << "MIDI event for channel " << channel << " (track "
			  << trackNum << ")" << endl;
		midiEvent->print();
                          */


                m_midiComposition[trackNum].push_back(midiEvent);

		if (midiEvent->getChannelNumber() == MIDI_PERCUSSION_CHANNEL) {
		    m_percussionTracks.insert(trackNum);
		}

                break;

            case MIDI_PITCH_BEND:
                data2 = getMIDIByte();

                // create and store our event
                midiEvent = new MIDIEvent(deltaTime, eventCode, data1, data2);
                m_midiComposition[trackNum].push_back(midiEvent);
                break;

            case MIDI_PROG_CHANGE:
            case MIDI_CHNL_AFTERTOUCH:
                // create and store our event
                midiEvent = new MIDIEvent(deltaTime, eventCode, data1);
                m_midiComposition[trackNum].push_back(midiEvent);
                break;

            case MIDI_SYSTEM_EXCLUSIVE:
                messageLength = getNumberFromMIDIBytes(data1);

#ifdef MIDI_DEBUG
		cerr << "SysEx of " << messageLength << " bytes found" << endl;
#endif

                metaMessage= getMIDIBytes(messageLength);

                if (MIDIByte(metaMessage[metaMessage.length() - 1]) !=
                        MIDI_END_OF_EXCLUSIVE)
                {
#ifdef MIDI_DEBUG
                    cerr << "MIDIFileReader::parseTrack() - "
                              << "malformed or unsupported SysEx type"
                              << endl;
#endif
                    continue;
                }

                // chop off the EOX 
                // length fixed by Pedro Lopez-Cabanillas (20030523)
                //
                metaMessage = metaMessage.substr(0, metaMessage.length()-1);

                midiEvent = new MIDIEvent(deltaTime,
                                          MIDI_SYSTEM_EXCLUSIVE,
                                          metaMessage);
                m_midiComposition[trackNum].push_back(midiEvent);
                break;

            case MIDI_END_OF_EXCLUSIVE:
#ifdef MIDI_DEBUG
                cerr << "MIDIFileReader::parseTrack() - "
                          << "Found a stray MIDI_END_OF_EXCLUSIVE" << endl;
#endif
                break;

            default:
#ifdef MIDI_DEBUG
                cerr << "MIDIFileReader::parseTrack()" 
                          << " - Unsupported MIDI Event Code:  "
                          << (int)eventCode << endl;
#endif
                break;
            } 
        }
    }

    if (lastTrackNum > metaTrack) {
	for (unsigned int track = metaTrack + 1; track <= lastTrackNum; ++track) {
	    m_trackNames[track] = QString("%1 <%2>")
		.arg(m_trackNames[metaTrack]).arg(track - metaTrack + 1);
	}
    }

    return true;
}

// Delete dead NOTE OFF and NOTE ON/Zero Velocity Events after
// reading them and modifying their relevant NOTE ONs.  Return true
// if there are some notes in this track.
//
bool
MIDIFileReader::consolidateNoteOffEvents(unsigned int track)
{
    bool notesOnTrack = false;
    bool noteOffFound;

    for (MIDITrack::iterator i = m_midiComposition[track].begin();
	 i != m_midiComposition[track].end(); i++) {

        if ((*i)->getMessageType() == MIDI_NOTE_ON && (*i)->getVelocity() > 0) {

	    notesOnTrack = true;
            noteOffFound = false;

            for (MIDITrack::iterator j = i;
		 j != m_midiComposition[track].end(); j++) {

                if (((*j)->getChannelNumber() == (*i)->getChannelNumber()) &&
		    ((*j)->getPitch() == (*i)->getPitch()) &&
                    ((*j)->getMessageType() == MIDI_NOTE_OFF ||
                    ((*j)->getMessageType() == MIDI_NOTE_ON &&
                     (*j)->getVelocity() == 0x00))) {

                    (*i)->setDuration((*j)->getTime() - (*i)->getTime());

                    delete *j;
                    m_midiComposition[track].erase(j);

                    noteOffFound = true;
                    break;
                }
            }

            // If no matching NOTE OFF has been found then set
            // Event duration to length of track
            //
            if (!noteOffFound) {
		MIDITrack::iterator j = m_midiComposition[track].end();
		--j;
                (*i)->setDuration((*j)->getTime()  - (*i)->getTime());
	    }
        }
    }

    return notesOnTrack;
}

// Add any tempo events found in the given track to the global tempo map.
//
void
MIDIFileReader::updateTempoMap(unsigned int track)
{
    std::cerr << "updateTempoMap for track " << track << " (" << m_midiComposition[track].size() << " events)" << std::endl;

    for (MIDITrack::iterator i = m_midiComposition[track].begin();
	 i != m_midiComposition[track].end(); ++i) {

        if ((*i)->isMeta() &&
	    (*i)->getMetaEventCode() == MIDI_SET_TEMPO) {

	    MIDIByte m0 = (*i)->getMetaMessage()[0];
	    MIDIByte m1 = (*i)->getMetaMessage()[1];
	    MIDIByte m2 = (*i)->getMetaMessage()[2];
	    
	    long tempo = (((m0 << 8) + m1) << 8) + m2;

	    std::cerr << "updateTempoMap: have tempo, it's " << tempo << " at " << (*i)->getTime() << std::endl;

	    if (tempo != 0) {
		double qpm = 60000000.0 / double(tempo);
		m_tempoMap[(*i)->getTime()] =
		    TempoChange(RealTime::zeroTime, qpm);
	    }
        }
    }
}

void
MIDIFileReader::calculateTempoTimestamps()
{
    unsigned long lastMIDITime = 0;
    RealTime lastRealTime = RealTime::zeroTime;
    double tempo = 120.0;
    int td = m_timingDivision;
    if (td == 0) td = 96;

    for (TempoMap::iterator i = m_tempoMap.begin(); i != m_tempoMap.end(); ++i) {
	
	unsigned long mtime = i->first;
	unsigned long melapsed = mtime - lastMIDITime;
	double quarters = double(melapsed) / double(td);
	double seconds = (60.0 * quarters) / tempo;

	RealTime t = lastRealTime + RealTime::fromSeconds(seconds);

	i->second.first = t;

	lastRealTime = t;
	lastMIDITime = mtime;
	tempo = i->second.second;
    }
}

RealTime
MIDIFileReader::getTimeForMIDITime(unsigned long midiTime) const
{
    unsigned long tempoMIDITime = 0;
    RealTime tempoRealTime = RealTime::zeroTime;
    double tempo = 120.0;

    TempoMap::const_iterator i = m_tempoMap.lower_bound(midiTime);
    if (i != m_tempoMap.begin()) {
	--i;
	tempoMIDITime = i->first;
	tempoRealTime = i->second.first;
	tempo = i->second.second;
    }

    int td = m_timingDivision;
    if (td == 0) td = 96;

    unsigned long melapsed = midiTime - tempoMIDITime;
    double quarters = double(melapsed) / double(td);
    double seconds = (60.0 * quarters) / tempo;

/*
    std::cerr << "MIDIFileReader::getTimeForMIDITime(" << midiTime << ")"
	      << std::endl;
    std::cerr << "timing division = " << td << std::endl;
    std::cerr << "nearest tempo event (of " << m_tempoMap.size() << ") is at " << tempoMIDITime << " ("
	      << tempoRealTime << ")" << std::endl;
    std::cerr << "quarters since then = " << quarters << std::endl;
    std::cerr << "tempo = " << tempo << " quarters per minute" << std::endl;
    std::cerr << "seconds since then = " << seconds << std::endl;
    std::cerr << "resulting time = " << (tempoRealTime + RealTime::fromSeconds(seconds)) << std::endl;
*/

    return tempoRealTime + RealTime::fromSeconds(seconds);
}

Model *
MIDIFileReader::load() const
{
    if (!isOK()) return 0;

    if (m_loadableTracks.empty()) {
	QMessageBox::critical(0, tr("No notes in MIDI file"),
			      tr("MIDI file \"%1\" has no notes in any track")
			      .arg(m_path));
	return 0;
    }

    std::set<unsigned int> tracksToLoad;

    if (m_loadableTracks.size() == 1) {

	tracksToLoad.insert(*m_loadableTracks.begin());

    } else {

	QStringList available;
	QString allTracks = tr("Merge all tracks");
	QString allNonPercussion = tr("Merge all non-percussion tracks");

	int nonTrackItems = 1;

	available << allTracks;

	if (!m_percussionTracks.empty() &&
	    (m_percussionTracks.size() < m_loadableTracks.size())) {
	    available << allNonPercussion;
	    ++nonTrackItems;
	}

	for (set<unsigned int>::iterator i = m_loadableTracks.begin();
	     i != m_loadableTracks.end(); ++i) {

	    unsigned int trackNo = *i;
	    QString label;

	    QString perc;
	    if (m_percussionTracks.find(trackNo) != m_percussionTracks.end()) {
		perc = tr(" - uses GM percussion channel");
	    }

	    if (m_trackNames.find(trackNo) != m_trackNames.end()) {
		label = tr("Track %1 (%2)%3")
		    .arg(trackNo).arg(m_trackNames.find(trackNo)->second)
		    .arg(perc);
	    } else {
		label = tr("Track %1 (untitled)%3").arg(trackNo).arg(perc);
	    }
	    available << label;
	}

	bool ok = false;
	QString selected = QInputDialog::getItem
	    (0, tr("Select track or tracks to import"),
	     tr("You can only import this file as a single annotation layer,\nbut the file contains more than one track,\nor notes on more than one channel.\n\nPlease select the track or merged tracks you wish to import:"),
	     available, 0, false, &ok);

	if (!ok || selected.isEmpty()) return 0;
	
	if (selected == allTracks || selected == allNonPercussion) {

	    for (set<unsigned int>::iterator i = m_loadableTracks.begin();
		 i != m_loadableTracks.end(); ++i) {
		
		if (selected == allTracks ||
		    m_percussionTracks.find(*i) == m_percussionTracks.end()) {

		    tracksToLoad.insert(*i);
		}
	    }

	} else {
	    
	    int j = nonTrackItems;

	    for (set<unsigned int>::iterator i = m_loadableTracks.begin();
		 i != m_loadableTracks.end(); ++i) {
		
		if (selected == available[j]) {
		    tracksToLoad.insert(*i);
		    break;
		}
		
		++j;
	    }
	}
    }

    if (tracksToLoad.empty()) return 0;

    size_t n = tracksToLoad.size(), count = 0;
    Model *model = 0;

    for (std::set<unsigned int>::iterator i = tracksToLoad.begin();
	 i != tracksToLoad.end(); ++i) {

	int minProgress = (100 * count) / n;
	int progressAmount = 100 / n;

	model = loadTrack(*i, model, minProgress, progressAmount);

	++count;
    }

    if (dynamic_cast<NoteModel *>(model)) {
	dynamic_cast<NoteModel *>(model)->setCompletion(100);
    }

    return model;
}

Model *
MIDIFileReader::loadTrack(unsigned int trackToLoad,
			  Model *existingModel,
			  int minProgress,
			  int progressAmount) const
{
    if (m_midiComposition.find(trackToLoad) == m_midiComposition.end()) {
	return 0;
    }

    NoteModel *model = 0;

    if (existingModel) {
	model = dynamic_cast<NoteModel *>(existingModel);
	if (!model) {
	    std::cerr << "WARNING: MIDIFileReader::loadTrack: Existing model given, but it isn't a NoteModel -- ignoring it" << std::endl;
	}
    }

    if (!model) {
	model = new NoteModel(m_mainModelSampleRate, 1, 0.0, 0.0, false);
	model->setValueQuantization(1.0);
    }

    const MIDITrack &track = m_midiComposition.find(trackToLoad)->second;

    size_t totalEvents = track.size();
    size_t count = 0;

    bool minorKey = false;
    bool sharpKey = true;

    for (MIDITrack::const_iterator i = track.begin(); i != track.end(); ++i) {

	RealTime rt = getTimeForMIDITime((*i)->getTime());

	// We ignore most of these event types for now, though in
	// theory some of the text ones could usefully be incorporated

	if ((*i)->isMeta()) {

	    switch((*i)->getMetaEventCode()) {

	    case MIDI_KEY_SIGNATURE:
		minorKey = (int((*i)->getMetaMessage()[1]) != 0);
		sharpKey = (int((*i)->getMetaMessage()[0]) >= 0);
		break;

	    case MIDI_TEXT_EVENT:
	    case MIDI_LYRIC:
	    case MIDI_TEXT_MARKER:
	    case MIDI_COPYRIGHT_NOTICE:
	    case MIDI_TRACK_NAME:
		// The text events that we could potentially use
		break;

	    case MIDI_SET_TEMPO:
		// Already dealt with in a separate pass previously
		break;

	    case MIDI_TIME_SIGNATURE:
		// Not yet!
		break;

	    case MIDI_SEQUENCE_NUMBER:
	    case MIDI_CHANNEL_PREFIX_OR_PORT:
	    case MIDI_INSTRUMENT_NAME:
	    case MIDI_CUE_POINT:
	    case MIDI_CHANNEL_PREFIX:
	    case MIDI_SEQUENCER_SPECIFIC:
	    case MIDI_SMPTE_OFFSET:
	    default:
		break;
	    }

	} else {

	    switch ((*i)->getMessageType()) {

	    case MIDI_NOTE_ON:

                if ((*i)->getVelocity() == 0) break; // effective note-off
		else {
		    RealTime endRT = getTimeForMIDITime((*i)->getTime() +
							(*i)->getDuration());

		    long startFrame = RealTime::realTime2Frame
			(rt, model->getSampleRate());

		    long endFrame = RealTime::realTime2Frame
			(endRT, model->getSampleRate());

		    QString pitchLabel = Pitch::getPitchLabel((*i)->getPitch(),
							      0, 
							      !sharpKey);

		    QString noteLabel = tr("%1 - vel %2")
			.arg(pitchLabel).arg(int((*i)->getVelocity()));

		    Note note(startFrame, (*i)->getPitch(),
			      endFrame - startFrame, noteLabel);

//		    std::cerr << "Adding note " << startFrame << "," << (endFrame-startFrame) << " : " << int((*i)->getPitch()) << std::endl;

		    model->addPoint(note);
		    break;
		}

            case MIDI_PITCH_BEND:
		// I guess we could make some use of this...
                break;

            case MIDI_NOTE_OFF:
            case MIDI_PROG_CHANGE:
            case MIDI_CTRL_CHANGE:
            case MIDI_SYSTEM_EXCLUSIVE:
            case MIDI_POLY_AFTERTOUCH:
            case MIDI_CHNL_AFTERTOUCH:
                break;

            default:
                break;
            }
	}

	model->setCompletion(minProgress +
			     (count * progressAmount) / totalEvents);
	++count;
    }

    return model;
}

