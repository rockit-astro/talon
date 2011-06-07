/*
	main.cpp - This file is a test of the CSBIGCam and CSBIGImage
			   classes.

	1. This software (c)2004 Santa Barbara Instrument Group.
	2. This free software is provided as an example of how 
	   to communicate with SBIG cameras.  It is provided AS-IS
	   without any guarantees by SBIG of suitability for a 
	   particular purpose and without any guarantee to be 
	   bug-free.  If you use it you agree to these terms and
	   agree to do so at your own risk.
    3. Any distribution of this source code to include these
	   terms.

	Revision History
	Date		Modification
	=========================================================
	1/26/04		Initial release

*/
#ifdef HAVE_CONFIG_H
 #include <config.h>
#endif

#include <stdlib.h>
#include <iostream>
#include <string>

using namespace std;

#include "csbigcam.h"
#include "csbigimg.h"

#define LINE_LEN 80

int main(/*int argc, char *argv[]*/)
{
	CSBIGCam *pCam = (CSBIGCam *)0;
	CSBIGCam *pCam2 = (CSBIGCam *)0;
	CSBIGImg *pImg = (CSBIGImg *)0;
	CSBIGImg *pImg2 = (CSBIGImg *)0;
	PAR_ERROR err;
	SBIG_FILE_ERROR ferr;
	char s[LINE_LEN];

	do { // allow break out
		cout << "Creating the SBIGCam Object on USB..." << endl;
		pCam = new CSBIGCam(DEV_USB);
		if ( (err = pCam->GetError()) != CE_NO_ERROR )
			break;
		cout << "Establishing a Link to the USB Camera..." << endl;
		if ( (err = pCam->EstablishLink()) != CE_NO_ERROR )
			break;
		cout << "Link Established to Camera Type: " << pCam->GetCameraTypeString() << endl;
		cout << "Taking light image on USB..." << endl;
		pImg = new CSBIGImg;
		if ( (err=pCam->GrabImage(pImg, SBDF_LIGHT_ONLY)) != CE_NO_ERROR )
			break;
		cout << "Saving compressed image..." << endl;
		if ( (ferr = pImg->SaveImage("img1.sbig", SBIF_COMPRESSED)) != SBFE_NO_ERROR )
			break;		

		cout << "Creating 2nd SBIGCam Object on LPT..." << endl;
		pCam2 = new CSBIGCam(DEV_LPT1);
		if ( (err = pCam2->GetError()) != CE_NO_ERROR )
			break;
		cout << "Establishing a Link to the LPT Camera..." << endl;
		if ( (err = pCam2->EstablishLink()) != CE_NO_ERROR )
			break;
		cout << "Link Established to Camera Type: " << pCam2->GetCameraTypeString() << endl;
		cout << "Taking light image on LPT..." << endl;
		pImg2 = new CSBIGImg;
		if ( (err=pCam2->GrabImage(pImg2, SBDF_LIGHT_ONLY)) != CE_NO_ERROR )
			break;
		cout << "Saving uncompressed image..." << endl;
		if ( (ferr = pImg2->SaveImage("img2.sbig", SBIF_UNCOMPRESSED)) != SBFE_NO_ERROR )
			break;

		cout << "Closing Devices..." << endl;
		if ( (err = pCam->CloseDevice()) != CE_NO_ERROR )
			break;
		if ( (err = pCam2->CloseDevice()) != CE_NO_ERROR )
			break;
		cout << "Closing Drivers..." << endl;
		if ( (err = pCam->CloseDriver()) != CE_NO_ERROR )
			break;		
		if ( (err = pCam2->CloseDriver()) != CE_NO_ERROR )
			break;
	} while (0);
	if ( err != CE_NO_ERROR )
		cout << "Camera Error: " << pCam->GetErrorString(err) << endl;
	else if ( ferr != SBFE_NO_ERROR )
		cout << "File Error: " << ferr << endl;
	else
		cout << "SUCCESS" << endl;
	cout << "Hit any key to continue:";
	cin.getline(s, LINE_LEN);;
	delete pCam;
	delete pCam2;
	delete pImg;
	delete pImg2;
	return EXIT_SUCCESS;
}
