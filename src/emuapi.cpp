#include <malloc.h>
#include "emuapi.h"

static int malbufpt = 0;
static char malbuf[EXTRA_HEAP];

void * emu_Malloc(unsigned int size)
{
  void * retval =  malloc(size);
  if (!retval) {

    if ( (malbufpt+size) < sizeof(malbuf) ) {
      retval = (void *)&malbuf[malbufpt];
      malbufpt += size;      
    }

  }
  
  return retval;
}

