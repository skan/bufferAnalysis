
/*!
 ***********************************************************************
 *  \file
 *     decoder_test.c
 *  \brief
 *     H.264/AVC decoder test 
 *  \author
 *     Main contributors (see contributors.h for copyright, address and affiliation details)
 *     - Yuwen He       <yhe@dolby.com>
 ***********************************************************************
 */

#include "contributors.h"

#include <sys/stat.h>

//#include "global.h"
#include "win32.h"
#include "h264decoder.h"
#include "configfile.h"
#include "crc.h"

#define DECOUTPUT_TEST      0

#define PRINT_OUTPUT_POC    0
#define BITSTREAM_FILENAME  "test.264"
#define DECRECON_FILENAME   "test_dec.yuv"
#define ENCRECON_FILENAME   "test_rec.yuv"
#define FCFR_DEBUG_FILENAME "fcfr_dec_rpu_stats.txt"
#define DECOUTPUT_VIEW0_FILENAME  "H264_Decoder_Output_View0.yuv"
#define DECOUTPUT_VIEW1_FILENAME  "H264_Decoder_Output_View1.yuv"

// HVA
#define MAX(x,y) ((x) < (y) ? (y) : (x))
#define MIN(x,y) ((x) < (y) ? (x) : (y))
int hvaNalCounter;
hvaNalDetails_t hvaNalDetails [HVA_MAX_NAL_NUMBER];
hvaAuDetails_t hvaAuDetails [HVA_MAX_AU_NUMBER];
hvaParseData_t hvaParseData;
hvaResults_t hvaResults;
int targetBitRate = 1500000;

frameDetails_t frameDetailx [MAX_FRAME_NUMBER];

static void Configure(InputParameters *p_Inp, int ac, char *av[])
{
  //char *config_filename=NULL;
  //char errortext[ET_SIZE];
  memset(p_Inp, 0, sizeof(InputParameters));
  strcpy(p_Inp->infile, BITSTREAM_FILENAME); //! set default bitstream name
  strcpy(p_Inp->outfile, DECRECON_FILENAME); //! set default output file name
  strcpy(p_Inp->reffile, ENCRECON_FILENAME); //! set default reference file name
  
#ifdef _LEAKYBUCKET_
  strcpy(p_Inp->LeakyBucketParamFile,"leakybucketparam.cfg");    // file where Leaky Bucket parameters (computed by encoder) are stored
#endif

  ParseCommand(p_Inp, ac, av);

  fprintf(stdout,"----------------------------- JM %s %s -----------------------------\n", VERSION, EXT_VERSION);
  //fprintf(stdout," Decoder config file                    : %s \n",config_filename);
  if(!p_Inp->bDisplayDecParams)
  {
    fprintf(stdout,"--------------------------------------------------------------------------\n");
    fprintf(stdout," Input H.264 bitstream                  : %s \n",p_Inp->infile);
    fprintf(stdout," Output decoded YUV                     : %s \n",p_Inp->outfile);
    //fprintf(stdout," Output status file                     : %s \n",LOGFILE);
    fprintf(stdout," Input reference file                   : %s \n",p_Inp->reffile);

    fprintf(stdout,"--------------------------------------------------------------------------\n");
  #ifdef _LEAKYBUCKET_
    fprintf(stdout," Rate_decoder        : %8ld \n",p_Inp->R_decoder);
    fprintf(stdout," B_decoder           : %8ld \n",p_Inp->B_decoder);
    fprintf(stdout," F_decoder           : %8ld \n",p_Inp->F_decoder);
    fprintf(stdout," LeakyBucketParamFile: %s \n",p_Inp->LeakyBucketParamFile); // Leaky Bucket Param file
    calc_buffer(p_Inp);
    fprintf(stdout,"--------------------------------------------------------------------------\n");
  #endif
  }
  
}

/*********************************************************
if bOutputAllFrames is 1, then output all valid frames to file onetime; 
else output the first valid frame and move the buffer to the end of list;
*********************************************************/
static int WriteOneFrame(DecodedPicList *pDecPic, int hFileOutput0, int hFileOutput1, int bOutputAllFrames)
{
  int iOutputFrame=0;
  DecodedPicList *pPic = pDecPic;

  if(pPic && (((pPic->iYUVStorageFormat==2) && pPic->bValid==3) || ((pPic->iYUVStorageFormat!=2) && pPic->bValid==1)) )
  {
    int i, iWidth, iHeight, iStride, iWidthUV, iHeightUV, iStrideUV;
    byte *pbBuf;    
    int hFileOutput;
    int res;

    iWidth = pPic->iWidth*((pPic->iBitDepth+7)>>3);
    iHeight = pPic->iHeight;
    iStride = pPic->iYBufStride;
    if(pPic->iYUVFormat != YUV444)
      iWidthUV = pPic->iWidth>>1;
    else
      iWidthUV = pPic->iWidth;
    if(pPic->iYUVFormat == YUV420)
      iHeightUV = pPic->iHeight>>1;
    else
      iHeightUV = pPic->iHeight;
    iWidthUV *= ((pPic->iBitDepth+7)>>3);
    iStrideUV = pPic->iUVBufStride;
    
    do
    {
      if(pPic->iYUVStorageFormat==2)
        hFileOutput = (pPic->iViewId&0xffff)? hFileOutput1 : hFileOutput0;
      else
        hFileOutput = hFileOutput0;
      if(hFileOutput >=0)
      {
        //Y;
        pbBuf = pPic->pY;
        for(i=0; i<iHeight; i++)
        {
          res = write(hFileOutput, pbBuf+i*iStride, iWidth);
          if (-1==res)
          {
            error ("error writing to output file.", 600);
          }
        }

        if(pPic->iYUVFormat != YUV400)
        {
         //U;
         pbBuf = pPic->pU;
         for(i=0; i<iHeightUV; i++)
         {
           res = write(hFileOutput, pbBuf+i*iStrideUV, iWidthUV);
           if (-1==res)
           {
             error ("error writing to output file.", 600);
           }
}
         //V;
         pbBuf = pPic->pV;
         for(i=0; i<iHeightUV; i++)
         {
           res = write(hFileOutput, pbBuf+i*iStrideUV, iWidthUV);
           if (-1==res)
           {
             error ("error writing to output file.", 600);
           }
         }
        }

        iOutputFrame++;
      }

      if (pPic->iYUVStorageFormat == 2)
      {
        hFileOutput = ((pPic->iViewId>>16)&0xffff)? hFileOutput1 : hFileOutput0;
        if(hFileOutput>=0)
        {
          int iPicSize =iHeight*iStride;
          //Y;
          pbBuf = pPic->pY+iPicSize;
          for(i=0; i<iHeight; i++)
          {
            res = write(hFileOutput, pbBuf+i*iStride, iWidth);
            if (-1==res)
            {
              error ("error writing to output file.", 600);
            }
          }

          if(pPic->iYUVFormat != YUV400)
          {
           iPicSize = iHeightUV*iStrideUV;
           //U;
           pbBuf = pPic->pU+iPicSize;
           for(i=0; i<iHeightUV; i++)
           {
             res = write(hFileOutput, pbBuf+i*iStrideUV, iWidthUV);
             if (-1==res)
             {
               error ("error writing to output file.", 600);
             }
           }
           //V;
           pbBuf = pPic->pV+iPicSize;
           for(i=0; i<iHeightUV; i++)
           {
             res = write(hFileOutput, pbBuf+i*iStrideUV, iWidthUV);
             if (-1==res)
             {
               error ("error writing to output file.", 600);
             }
           }
          }

          iOutputFrame++;
        }
      }

#if PRINT_OUTPUT_POC
      fprintf(stdout, "\nOutput frame: %d/%d\n", pPic->iPOC, pPic->iViewId);
#endif
      pPic->bValid = 0;
      pPic = pPic->pNext;
    }while(pPic != NULL && pPic->bValid && bOutputAllFrames);
  }
#if PRINT_OUTPUT_POC
  else
    fprintf(stdout, "\nNone frame output\n");
#endif

  return iOutputFrame;
}

/*!
 ***********************************************************************
 * \brief
 *    main function for JM decoder
 ***********************************************************************
 */
int main(int argc, char **argv)
{
  int iRet;
  DecodedPicList *pDecPicList;
  int hFileDecOutput0=-1, hFileDecOutput1=-1;
  int iFramesOutput=0, iFramesDecoded=0;
  InputParameters InputParams;


#if DECOUTPUT_TEST
  hFileDecOutput0 = open(DECOUTPUT_VIEW0_FILENAME, OPENFLAGS_WRITE, OPEN_PERMISSIONS);
  fprintf(stdout, "Decoder output view0: %s\n", DECOUTPUT_VIEW0_FILENAME);
  hFileDecOutput1 = open(DECOUTPUT_VIEW1_FILENAME, OPENFLAGS_WRITE, OPEN_PERMISSIONS);
  fprintf(stdout, "Decoder output view1: %s\n", DECOUTPUT_VIEW1_FILENAME);
#endif

  // Validation metrics: init
  hvaNalCounter = 0;
  init_time();

  //get input parameters;
  Configure(&InputParams, argc, argv);
  //open decoder;
  iRet = OpenDecoder(&InputParams);
  if(iRet != DEC_OPEN_NOERR)
  {
    fprintf(stderr, "Open encoder failed: 0x%x!\n", iRet);
    return -1; //failed;
  }

  //decoding;
  do
  {
    iRet = DecodeOneFrame(&pDecPicList);
    if(iRet==DEC_EOS || iRet==DEC_SUCCEED)
    {
      //process the decoded picture, output or display;
      iFramesOutput += WriteOneFrame(pDecPicList, hFileDecOutput0, hFileDecOutput1, 0);
      iFramesDecoded++;
    }
    else
    {
      //error handling;
      fprintf(stderr, "Error in decoding process: 0x%x\n", iRet);
    }
  }while((iRet == DEC_SUCCEED) && ((p_Dec->p_Inp->iDecFrmNum==0) || (iFramesDecoded<p_Dec->p_Inp->iDecFrmNum)));

  hvaProcessMetrics();

  iRet = FinitDecoder(&pDecPicList);
  iFramesOutput += WriteOneFrame(pDecPicList, hFileDecOutput0, hFileDecOutput1 , 1);
  iRet = CloseDecoder();

  //quit;
  if(hFileDecOutput0>=0)
  {
    close(hFileDecOutput0);
  }
  if(hFileDecOutput1>=0)
  {
    close(hFileDecOutput1);
  }

  printf("%d frames are decoded.\n", iFramesDecoded);
  return 0;
}

void hvaProcessMetrics()
{
   int counter = 0;
   int picCounter = 0;
   int j = 0;
   size_t readInput;
   FILE *inputBitstream;
   FILE *outputCrcDump;
   int overallNalCount = hvaNalCounter;
   hvaCpb_t cpb[3000];


   inputBitstream = fopen(p_Dec->p_Inp->infile, "r");
   outputCrcDump = fopen("out.crc", "wb");

#if 0
   for (counter = 0 ; counter <= overallNalCount ; counter++)
   {
      printf ("SKH debug: counter = %d; picNumber = %d; type = %d ; length = %d ; position = %lu \n",
            counter,
            hvaNalDetails[counter].picNumber,
            hvaNalDetails[counter].type,
            hvaNalDetails[counter].size, 
            hvaNalDetails[counter].position);
   }
#endif
   counter = 0;
   while( counter < overallNalCount ) /*Delimit access units*/
   {
      j = counter;
      while (j < overallNalCount && (hvaNalDetails[counter].picNumber == hvaNalDetails[j].picNumber))
      {
         hvaAuDetails[picCounter].size += hvaNalDetails[j].size;
         j++;
      }
//      hvaAuDetails[picCounter].size = hvaNalDetails[j].position - hvaNalDetails[counter].position;
      hvaAuDetails[picCounter].number = hvaNalDetails[counter].picNumber;
      hvaAuDetails[picCounter].position = hvaNalDetails[counter].position;
      picCounter = hvaNalDetails[counter].picNumber;
      counter = j;
   }
   
   for (counter = 0; counter < picCounter ; counter++)
   {
      readInput = (char*)malloc (sizeof(char) * hvaAuDetails[counter].size);
      fseek (inputBitstream, hvaAuDetails[counter].position, SEEK_SET);
      fread (readInput, sizeof(char), hvaAuDetails[counter].size, inputBitstream);
      hvaAuDetails[counter].crc = crc (readInput, hvaAuDetails[counter].size);
#if 0
      printf ("SKH AU debug: counter = %d; picNumber = %d; length = %d ; position = %lu ; CRC = %lu\n",
            counter,
            hvaAuDetails[counter].number,
            hvaAuDetails[counter].size,
            hvaAuDetails[counter].position,
            hvaAuDetails[counter].crc);
#endif
      fprintf(outputCrcDump,"%lu\n", hvaAuDetails[counter].crc);
   }
   fclose (inputBitstream);
   fclose (outputCrcDump);
   printf("skh debug : time scale = %d\n", p_Dec->p_Vid->active_sps->vui_seq_parameters.time_scale);
   printf("skh debug : num_unit_ticks = %d\n", p_Dec->p_Vid->active_sps->vui_seq_parameters.num_units_in_tick);
   printf("skh debug : delay = %d\n", hvaParseData.SeiInitialDelay);

   hvaResults.averageBitRate = hvaComputeAverageBitRate(picCounter);
   hvaAnalyseBuffer(picCounter, cpb);
   printf("skh debug results: average bitrate = %d", hvaResults.averageBitRate);

}

/*********************************************
* computeAverageBitRate
* *******************************************/
int hvaComputeAverageBitRate(int framesDecoded)
{
   unsigned int i;
   int bitRatePerPicture;
   unsigned long cumulatedBitPerPicture;
   int frameRate;

   frameRate = (p_Dec->p_Vid->active_sps->vui_seq_parameters.time_scale / p_Dec->p_Vid->active_sps->vui_seq_parameters.num_units_in_tick) / 2;
   printf("skh debug : frameRate = %d\n", frameRate);

   for (i = 0; i < framesDecoded ; i++)
   {
      bitRatePerPicture = (hvaAuDetails[i].size*frameRate*8);
      cumulatedBitPerPicture += bitRatePerPicture;
      //printf("sk test: %d, %d, %d, %d\n",i,frame[i].esSize,bitRatePerPicture);
   }
   return  (cumulatedBitPerPicture / framesDecoded) ;
}

/*********************************************
 * analyseBuffer 
 * *******************************************/
int hvaAnalyseBuffer(int numberOfPicts, hvaCpb_t * cpb)
{
   int i = 0;
   int j = 0;
   int time = 0;
   int frameRate = (p_Dec->p_Vid->active_sps->vui_seq_parameters.time_scale / p_Dec->p_Vid->active_sps->vui_seq_parameters.num_units_in_tick) / 2;
   int vSync = 1000 / frameRate ; /* (vSync in ms) */ 
   int totalTime = vSync * numberOfPicts;
   int initialDelay = hvaParseData.SeiInitialDelay / 90 ;
   int cpbSize = hvaParseData.cpb_size_value_minus1 * 32;

   printf("skh debug : hvaParseData.SeiInitialDelay = %d\n", initialDelay);
   printf("skh debug: cpbSize = %d \n", cpbSize);
   hvaResults.targetBitRate=hvaParseData.bit_rate_value_minus1 * 64;
   hvaResults.initialDelay = hvaParseData.SeiInitialDelay / 90;
   printf("skh debug : hvaResults.targetBitRate = %d\n", hvaResults.targetBitRate);
   
   for (time = 0 ; time < totalTime ; time +=vSync)
   {
      i++;
      cpb[i].fullNess = cpb[i-1].fullNess + (hvaResults.targetBitRate / frameRate);
      cpb[i].time = time;
      cpb[i].index=i;
      hvaResults.maxCpbFullness = MAX(hvaResults.maxCpbFullness,cpb[i].fullNess);
      if (time >= hvaResults.initialDelay )
      {
         i++;
         cpb[i].fullNess = cpb[i-1].fullNess - 8*hvaAuDetails[j].size;
         cpb[i].time = time;
         cpb[i].index=i;
         if (hvaResults.minCpbFullness == 0)
            hvaResults.minCpbFullness = cpb[i].fullNess;
         else
            hvaResults.minCpbFullness = MIN(hvaResults.minCpbFullness,cpb[i].fullNess);
         j++;
      }
   }
#if 1 /*SKH debug*/
   for (j=0; j < i; j++)
      printf("%d;%d\n", cpb[j].time, cpb[j].fullNess);
#endif 
   return i;
}
/*********************************************
 * compute Target Bitrate
 * *******************************************/
int hvaComputeTargetBitrate ()
{
   int targetBitRate = 0;
   if (p_Dec->p_Vid->width <= 480)
      targetBitRate = 300000;
   else if (p_Dec->p_Vid->width <= 719)
      targetBitRate = 1500000;
   else if (p_Dec->p_Vid->width <= 1079)
      targetBitRate = 4000000;
   else
      targetBitRate = 8000000;
   return targetBitRate; 
}

