/* Declare a list of the getCallbacksCCD() functions for each available
 * camera driver */

/* Cameras are detected in this order. If you add support for a new camera,
   you must add your getCallbacksCCD function to this list.
 */
void (*camera_drivers[])(CCDCallbacks*) = {

};

