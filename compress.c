/*! compress.c
 *  Compressor logic
 *
 *  (c)2002-2007 busybee (http://beesbuzz.biz/
 *  Licensed under the terms of the LGPL. See the file COPYING for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "compress.h"

//! Private state container struct
struct Compressor {
        //! The compressor's preferences
        struct CompressorConfig prefs;
        
        //! History of the peak values
        int *peaks;
                
        //! History of the gain values
        int lastGain;

        //The maximum gain reached for all the session
        int minGain;
        
        unsigned int pos;
        unsigned int bufsz;
};

struct Compressor *Compressor_new(unsigned int history)
{
        struct Compressor *obj = malloc(sizeof(struct Compressor));

        obj->prefs.target = TARGET;
        obj->prefs.maxgain = GAINMAX;
        obj->prefs.smooth = GAINSMOOTH;

        obj->peaks = NULL;
        obj->bufsz = 0;
        obj->pos = 0;
        obj->minGain = -1;
        obj->lastGain = 0;
        
        Compressor_setHistory(obj, history);
        
        return obj;
}

void Compressor_delete(struct Compressor *obj)
{
        if (obj->peaks)
                free(obj->peaks);
        free(obj);
}

static int *resizeArray(int *data, int newsz, int oldsz, int* newPos)
{
        if (newsz < oldsz) {
            int *tmp = realloc(NULL, newsz*sizeof(int));
            int size_after = newsz - *newPos;
            size_after = (size_after < 0)?0:size_after;
            int size_before = newsz - size_after;
            memcpy((void*)&tmp[0], (void*)&(data[oldsz - 1 - size_after]), size_after*sizeof(int));
            memcpy((void*)&(tmp[size_after]), (void*)&(data[*newPos - size_before]), size_before*sizeof(int));
            free(data);
            data = tmp;
            *newPos = 0;
        } else {
            data = realloc(data, newsz*sizeof(int));
            memset(data + oldsz, 0, sizeof(int)*(newsz - oldsz));
        }
        return data;
}

void Compressor_setHistory(struct Compressor *obj, unsigned int history)
{
        if (!history)
                history = BUCKETS;
        
        obj->peaks = resizeArray(obj->peaks, history, obj->bufsz, &obj->pos);
        obj->minGain = -1;
        obj->bufsz = history;
}

struct CompressorConfig *Compressor_getConfig(struct Compressor *obj)
{
        return &obj->prefs;
}

void Compressor_Process_int16(struct Compressor *obj, int16_t *audio, 
                              unsigned int count)
{
        struct CompressorConfig *prefs = Compressor_getConfig(obj);
        int16_t *ap;
        int i, p;
        int *peaks = obj->peaks;
        int minGain = obj->minGain;
        int lastGain = obj->lastGain;
        int newGain;
        int peakVal = 1;
        int peakPos = 0;
        int ramp = 512;
        int delta;
        int smooth = prefs->smooth;
        ap = audio;

        for (p = 0; p <= (count+511)/512; p++) {
            int packetCount = ((p * 512)<count)?512:((p*512) - count);
            if (packetCount == 0) return;
            int slot = (obj->pos + 1) % obj->bufsz;
            int16_t *peakap = ap;
            if (!lastGain)
                lastGain = 1 << 10;

            for (i = 0; i < packetCount; i++)
            {
                int val = *peakap++;
                if (val < 0)
                        val = -val;
                if (val > peakVal)
                {
                        peakVal = val;
                        peakPos = i;
                }
            }
            peaks[slot] = peakVal;
            int peakMax = peakVal;
            for (i = 0; i < obj->bufsz; i++)
            {
                if (peaks[i] > peakMax)
                {
                        peakMax = peaks[i];
                        peakPos = 0;
                }
            }

            //! Determine target gain
            if (peakMax > 20) {
                newGain = ((1 << 10)*prefs->target)/peakVal;
                minGain = (minGain<0)?newGain:minGain;
                minGain = (minGain > newGain)?newGain:minGain;
                //! Make sure it's no more than the maximum gain value
                if (newGain > minGain + (prefs->maxgain << 10))
                    newGain = minGain + (prefs->maxgain << 10);
            } else {
                newGain = ((1 << 10)*prefs->target/peakVal > (prefs->maxgain<< 10))?(prefs->maxgain<< 10):((1 << 10)*prefs->target)/peakVal;
            }

            //! Adjust the gain with inertia from the previous gain value
            newGain = (lastGain*smooth + newGain) / (smooth + 1);

            //! Make sure it's no less than 1:1
            if (newGain < (1 << 10))
                newGain = 1 << 10;

            //! Make sure the adjusted gain won't cause clipping
            if ((peakVal*newGain >> 10) > 32767)
            {
                newGain = (32767 << 10)/peakVal;
                //! Truncate the ramp time
                ramp = peakPos;
            }
            //! Record the new gain
            obj->lastGain = newGain;

            if (!ramp)
                ramp = 1;
            delta = (newGain - lastGain)/ramp;

            for (i = 0; i < packetCount; i++)
            {
                int sample;

                //! Amplify the sample
                sample = *ap*lastGain >> 10;
                if (sample < -32768)
                {
                        sample = -32768;
                } else if (sample > 32767)
                {
                        sample = 32767;
                }
                *ap++ = sample;

                //! Adjust the gain
                if (i < ramp)
                        lastGain += delta;
                else
                        lastGain = newGain;
            }

            obj->pos = slot;
            obj->minGain = minGain;
	}
}

