//
// pch.h
// En-t�te pour fichiers Include syst�me standard.
//
// Utilis� par le syst�me de g�n�ration pour g�n�rer l'en-t�te pr�compil�. Notez qu'aucun(e)
// pch.cpp n'est n�cessaire et que pch.h est automatiquement inclus dans tous les fichiers cpp
// qui font partie du projet.
//

#include <jni.h>
#include <errno.h>

#include <string.h>
#include <unistd.h>
#include <sys/resource.h>

#include <EGL/egl.h>
#include <GLES/gl.h>

#include <android/sensor.h>

#include <android/log.h>
#include "android_native_app_glue.h"
