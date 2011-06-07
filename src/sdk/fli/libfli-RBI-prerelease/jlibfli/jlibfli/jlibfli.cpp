// jlibfli.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "jlibfli.h"
#include "libfli.h"

#ifdef _MANAGED
#pragma managed(push, off)
#endif

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
    return TRUE;
}

#ifdef _MANAGED
#pragma managed(pop)
#endif

JNIEXPORT jstring JNICALL Java_JLibFLI_FLIGetLibVersion
	(JNIEnv *_env, jobject _obj)
{
	char libVer[1024];

	memset(libVer, '\0', sizeof(libVer));
	FLIGetLibVersion(libVer, sizeof(libVer) - 1);

	return _env->NewStringUTF(libVer);
}

JNIEXPORT jint JNICALL Java_JLibFLI_FLIOpen
  (JNIEnv* _env, jobject _obj, jstring _name, jint _domain)
{
	jint r;
	const char* name;
	flidev_t device = FLI_INVALID_DEVICE;
	
	name = _env->GetStringUTFChars(_name, NULL);

	r = FLIOpen(&device, (char *) name, _domain);

	_env->ReleaseStringUTFChars(_name, name);

	if (r != 0) /* An error occurred during open */
		r = FLI_INVALID_DEVICE;

	return r;
}

JNIEXPORT jint JNICALL Java_JLibFLI_FLIClose
  (JNIEnv *_env, jobject _obj, jint _device)
{
	jint r;

	r = FLIClose(_device);

	return r;
}

JNIEXPORT jstring JNICALL Java_JLibFLI_FLIGetModel
	(JNIEnv *_env, jobject _obj, jint _device)
{
	char Model[1024];

	memset(Model, '\0', sizeof(Model));
	FLIGetModel(_device, Model, sizeof(Model) - 1);

	return _env->NewStringUTF(Model);
}

JNIEXPORT jint JNICALL Java_JLibFLI_FLIGetHWRevision
  (JNIEnv *_env, jobject _obj, jint _device)
{
	jint r;
	long revision;

	r = FLIGetHWRevision(_device, &revision);

	if (r == 0) /* Success */
		r = (jint) revision;
	else
		r = -1;

	return r;
}

JNIEXPORT jint JNICALL Java_JLibFLI_FLIGetFWRevision
  (JNIEnv *_env, jobject _obj, jint _device)
{
	jint r;
	long revision;

	r = FLIGetFWRevision(_device, &revision);

	if (r == 0) /* Success */
		r = (jint) revision;
	else
		r = -1;

	return r;
}

JNIEXPORT jintArray JNICALL Java_JLibFLI_FLIGetArrayArea
  (JNIEnv *_env, jobject _obj, jint _device)
{
	jint r;
	jintArray iarr = _env->NewIntArray(4);
	long ar[4];

	memset(ar, 0x00, sizeof(ar));

	r = FLIGetArrayArea(_device, &ar[0], &ar[1], &ar[2], &ar[3]); 

	_env->SetIntArrayRegion(iarr, 0, 4, ar);

	return iarr;
}

JNIEXPORT jintArray JNICALL Java_JLibFLI_FLIGetVisibleArea
  (JNIEnv *_env, jobject _obj, jint _device)
{
	jint r;
	jintArray iarr = _env->NewIntArray(4);
	long ar[4];

	memset(ar, 0x00, sizeof(ar));

	r = FLIGetVisibleArea(_device, &ar[0], &ar[1], &ar[2], &ar[3]); 

	_env->SetIntArrayRegion(iarr, 0, 4, ar);

	return iarr;
}

JNIEXPORT jdoubleArray JNICALL Java_JLibFLI_FLIGetPixelSize
  (JNIEnv *_env, jobject _obj, jint _device)
{
	jint r;
	jdoubleArray farr = _env->NewDoubleArray(2);
	double p[2] = {0,0};

	r = FLIGetPixelSize(_device, &p[0], &p[1]); 

	_env->SetDoubleArrayRegion(farr, 0, 2, p);

	return farr;
}

JNIEXPORT jint JNICALL Java_JLibFLI_FLISetExposureTime
  (JNIEnv *_env, jobject _obj, jint _device, jint _exposure)
{
	jint r;

	r = FLISetExposureTime(_device, _exposure);

	return r;
}

JNIEXPORT jint JNICALL Java_JLibFLI_FLISetImageArea
  (JNIEnv *_env, jobject _obj, jint _device,
	jint _ul_x, jint _ul_y, jint _lr_x, jint _lr_y)
{
	jint r;

	r = FLISetImageArea(_device, _ul_x, _ul_y, _lr_x, _lr_y);

	return r;
}

JNIEXPORT jint JNICALL Java_JLibFLI_FLISetHBin
  (JNIEnv *_env, jobject _obj, jint _device, jint _bin)
{
	jint r;

	r = FLISetHBin(_device, _bin);

	return r;
}

JNIEXPORT jint JNICALL Java_JLibFLI_FLISetVBin
  (JNIEnv *_env, jobject _obj, jint _device, jint _bin)
{
	jint r;

	r = FLISetVBin(_device, _bin);

	return r;
}

JNIEXPORT jint JNICALL Java_JLibFLI_FLICancelExposure
  (JNIEnv *_env, jobject _obj, jint _device)
{
	jint r;

	r = FLICancelExposure(_device);

	return r;
}

JNIEXPORT jint JNICALL Java_JLibFLI_FLIGetExposureStatus
  (JNIEnv *_env, jobject _obj, jint _device)
{
	jint r;
	long status = 0;

	r = FLIGetExposureStatus(_device, &status);

	if (r == 0) /* Success */
		r = status;

	return r;
}

JNIEXPORT jint JNICALL Java_JLibFLI_FLISetTemperature
  (JNIEnv *_env, jobject _obj, jint _device, jdouble _t)
{
	jint r;

	r = FLISetTemperature(_device, _t);

	return r;
}

JNIEXPORT jdouble JNICALL Java_JLibFLI_FLIGetTemperature
  (JNIEnv *_env, jobject _obj, jint _device, jint _channel)
{
	jint r;
	jdouble t = 100;

	r = FLIReadTemperature(_device, _channel, &t);
	
	return t;	
}

JNIEXPORT jint JNICALL Java_JLibFLI_FLIExposeFrame
  (JNIEnv *_env, jobject _obj, jint _device)
{
	jint r;

	r = FLIExposeFrame(_device);

	return r;
}

JNIEXPORT jint JNICALL Java_JLibFLI_FLIGrabRow
  (JNIEnv *_env, jobject _obj, jint _device, jint _width, jshortArray _array)
{
	if ( (_width < 1) || (_width > 16384) )
		return -1;

	jint r;
	jshort *b = new jshort [_width];

	if (b == NULL) return -1;

	r = FLIGrabRow(_device, b, _width);

	_env->SetShortArrayRegion(_array, 0, _width, b);
	
	delete [] b;
	return r;
}

JNIEXPORT jint JNICALL Java_JLibFLI_FLISetFrameType
  (JNIEnv *_env, jobject _obj, jint _device, jint _frametype)
{
	jint r;

	r = FLISetFrameType(_device, _frametype);

	return r;
}

