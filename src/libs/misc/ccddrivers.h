/* Declare a list of the getCallbacksCCD() functions for each available
 * camera driver */

void apogee_getCallbacksCCD(CCDCallbacks *callbacks);
void aux_getCallbacksCCD(CCDCallbacks *callbacks);
void fli_getCallbacksCCD(CCDCallbacks *callbacks);
void ocaas_getCallbacksCCD(CCDCallbacks *callbacks);
void server_getCallbacksCCD(CCDCallbacks *callbacks);
void sbig_getCallbacksCCD(CCDCallbacks *callbacks);

/* Cameras are detected in this order. If you add support for a new camera,
   you must add your getCallbacksCCD function to this list.
 */
void (*camera_drivers[])(CCDCallbacks*) = {
    fli_getCallbacksCCD,
    sbig_getCallbacksCCD,
    apogee_getCallbacksCCD,
    aux_getCallbacksCCD,
    server_getCallbacksCCD,
    ocaas_getCallbacksCCD,
};

