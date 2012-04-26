/* vidinput_directx2.h
 *
 *
 * DirectShow2 Implementation for the H323Plus/Opal Project.
 *
 * Copyright (c) 2009 ISVO (Asia) Pte Ltd. All Rights Reserved.
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Alternatively, the contents of this file may be used under the terms
 * of the General Public License (the  "GNU License"), in which case the
 * provisions of GNU License are applicable instead of those
 * above. If you wish to allow use of your version of this file only
 * under the terms of the GNU License and not to allow others to use
 * your version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the GNU License. If you do not delete
 * the provisions above, a recipient may use your version of this file
 * under either the MPL or the GNU License."
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * Initial work sponsored by Requestec Ltd.
 *
 *
 * Contributor(s): ______________________________________.
 *
 * $Log: directshow.h,v $
 *
 *
 */

#include <ptlib/plugin.h>

#ifdef P_DIRECTSHOW2

//////////////////////////////////////////////////////////////////////
// Video Input device

/**This class defines a video input device using DirectShow.
 */
struct IGraphBuilder;
struct IMediaControl;
struct IBaseFilter;
struct ICaptureGraphBuilder;
struct ICaptureGraphBuilder2;
struct ISampleGrabber;
struct IPin;
struct ISampleGrabberCB;
struct IAMCameraControl;
class PVideoInputDevice_DirectShow2 : public PVideoInputDevice
{
  //PCLASSINFO(PVideoInputDevice_DirectShow2, PVideoInputDevice);

  public:
    /** Create a new video input device.
     */
    PVideoInputDevice_DirectShow2();

    /**Close the video input device on destruction.
      */
    ~PVideoInputDevice_DirectShow2();

    /** Is the device a camera, and obtain video
     */
    static PStringArray GetInputDeviceNames();

    virtual PStringArray GetDeviceNames() const
      { return GetInputDeviceNames(); }

	/**Retrieve a list of Device Capabilities
	  */
	static PBoolean GetDeviceCapabilities(
      const PString & deviceName,           ///< Name of device
	  Capabilities * caps                   ///< List of supported capabilities
	);

    /**Open the device given the device name.
      */
    virtual PBoolean Open(
      const PString & DeviceName,   ///< Device name to open
      PBoolean startImmediate = TRUE    ///< Immediately start device
    );

#ifndef _WIN32_WCE
    virtual PVideoInputControl * GetVideoInputControls();
#endif

    /**Determine if the device is currently open.
      */
    virtual PBoolean IsOpen();

    /**Close the device.
      */
    virtual PBoolean Close();

    /**Start the video device I/O.
      */
    virtual PBoolean Start();

    /**Stop the video device I/O capture.
      */
    virtual PBoolean Stop();

    /**Determine if the video device I/O capture is in progress.
      */
    virtual PBoolean IsCapturing();

    /**Set the colour format to be used.
       Note that this function does not do any conversion. If it returns TRUE
       then the video device does the colour format in native mode.

       To utilise an internal converter use the SetColourFormatConverter()
       function.

       Default behaviour sets the value of the colourFormat variable and then
       returns TRUE.
    */
    virtual PBoolean SetColourFormat(
      const PString & colourFormat ///< New colour format for device.
    );

    /**Set the video frame rate to be used on the device.

       Default behaviour sets the value of the frameRate variable and then
       returns TRUE.
    */
    virtual PBoolean SetFrameRate(
      unsigned rate  ///< Frames  per second
    );

    /**Set the frame size to be used.

       Note that devices may not be able to produce the requested size, and
       this function will fail.  See SetFrameSizeConverter().

       Default behaviour sets the frameWidth and frameHeight variables and
       returns TRUE.
    */
    virtual PBoolean SetFrameSize(
      unsigned width,   ///< New width of frame
      unsigned height   ///< New height of frame
    );

    /**Get the maximum frame size in bytes.

       Note a particular device may be able to provide variable length
       frames (eg motion JPEG) so will be the maximum size of all frames.
      */
    virtual PINDEX GetMaxFrameBytes();

    /**Grab a frame, after a delay as specified by the frame rate.
      */
    virtual PBoolean GetFrameData(
      BYTE * buffer,                 ///< Buffer to receive frame
      PINDEX * bytesReturned         ///< OPtional bytes returned.
    );

    /**Grab a frame. Do not delay according to the current frame rate parameter.
      */
    virtual PBoolean GetFrameDataNoDelay(
      BYTE * buffer,                 ///< Buffer to receive frame
      PINDEX * bytesReturned         ///< OPtional bytes returned.
    );

    /**Try all known video formats & see which ones are accepted by the video driver
     */
    virtual PBoolean TestAllFormats();

	/**Set the video channel (not used)
	  */
	virtual PBoolean SetChannel(int newChannel);

	/**Play States 
	*/
	enum PLAYSTATE {
		Stopped, 
		Paused, 
		Running, 
		Init
	};

  protected:

   /**Check the hardware can do the asked for size.

       Note that not all cameras can provide all frame sizes.
     */
    virtual PBoolean VerifyHardwareFrameSize(unsigned width, unsigned height);

	/** Set the Video Format for the defualt output from the WebCam
	  */
    PBoolean SetVideoFormat(IPin * pin);

	// DirectShow 
	IGraphBuilder           * m_pGB;        ///< Filter Graph
#ifndef _WIN32_WCE
	ICaptureGraphBuilder2   * m_pCap;       ///< Capture Builder
#else
	ICaptureGraphBuilder    * m_pCap;       ///< Capture Builder
#endif
	IBaseFilter             * m_pDF;        ///< WebCam Device Filter
	IPin                    * m_pCamOutPin; ///< WebCam output out -> Transform Input pin
	ISampleGrabber          * m_pGrabber;   ///< Sample Grabber
	ISampleGrabberCB        * m_pGrabberCB; ///< Sample Callback function
	IBaseFilter			    * m_pNull;      ///< Null Render filter
	IMediaControl           * m_pMC;        ///< Media control interface
	IAMCameraControl        * m_pCC;		///< Camera controls

	PLAYSTATE m_psCurrent;    ///< PlayState
	GUID colorGUID;           ///< GUID of the color format

    PMutex        lastFrameMutex;
	bool          checkVFlip;
	bool          startup;
	long		  pBufferSize;
	BYTE		  * pBuffer;

};


#ifndef _WIN32_WCE
// Video Input Control

struct IAMCameraControl;
class PVideoInputControl_DirectShow2 : public PVideoInputControl
{
  PCLASSINFO(PVideoInputControl_DirectShow2, PVideoInputControl);

public:
	PVideoInputControl_DirectShow2(IAMCameraControl * _pCC);

	// overrides
	virtual bool Pan(long value, bool absolute = false);
	virtual bool Tilt(long value, bool absolute = false);
	virtual bool Zoom(long value, bool absolute = false);

protected:
	IAMCameraControl        * t_pCC;

};

#endif // WCE

#endif // P_DIRECTSHOW2
