/**
 * @file    img_reduce.c
 * @brief   Image analysis functions
 * 
 * Misc image analysis functions
 *  
 * @author  O. Guyon
 * @date    10 Jul 2017
 *
 * 
 * @bug No known bugs.
 * 
 */

#include <stdint.h>
#include <unistd.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <err.h>
#include <fcntl.h>
#include <sched.h>
#include <ncurses.h>
#include <semaphore.h>

#include <fitsio.h>

#include "CommandLineInterface/CLIcore.h"
#include "00CORE/00CORE.h"
#include "COREMOD_memory/COREMOD_memory.h"
#include "COREMOD_iofits/COREMOD_iofits.h"
#include "COREMOD_arith/COREMOD_arith.h"
#include "image_filter/image_filter.h"
#include "fft/fft.h"


#include "img_reduce/img_reduce.h"

# ifdef _OPENMP
# include <omp.h>
#define OMP_NELEMENT_LIMIT 1000000
# endif


/** image analysis/reduction routines for astronomy
 * 
 * 
 */


int badpixclean_init = 0;
long badpixclean_NBop;
long *badpixclean_array_indexin;
long *badpixclean_array_indexout;
float *badpixclean_array_coeff;

long badpixclean_NBbadpix;
long *badpixclean_indexlist;



extern DATA data;

static int INITSTATUS_img_reduce = 0;


// CLI commands
//
// function CLI_checkarg used to check arguments
// 1: float
// 2: long
// 3: string
// 4: existing image
//



int_fast8_t IMG_REDUCE_cubesimplestat_cli()
{
    if(CLI_checkarg(1,4)==0)
        IMG_REDUCE_cubesimplestat(data.cmdargtoken[1].val.string);
    else
        return 1;

    return(0);
}

int_fast8_t IMG_REDUCE_cubeprocess_cli()
{
    if(CLI_checkarg(1,4)==0)
        IMG_REDUCE_cubeprocess(data.cmdargtoken[1].val.string);
    else
        return 1;

    return(0);
}


int_fast8_t IMG_REDUCE_cleanbadpix_fast_cli()
{
    if(CLI_checkarg(1,4)+CLI_checkarg(2,4)+CLI_checkarg(3,3)==0)
        IMG_REDUCE_cleanbadpix_fast(data.cmdargtoken[1].val.string, data.cmdargtoken[2].val.string, data.cmdargtoken[3].val.string, 0);
    else
        return 1;

    return(0);
}

int_fast8_t IMG_REDUCE_cleanbadpix_stream_fast_cli()
{
    if(CLI_checkarg(1,4)+CLI_checkarg(2,4)+CLI_checkarg(3,3)==0)
        IMG_REDUCE_cleanbadpix_fast(data.cmdargtoken[1].val.string, data.cmdargtoken[2].val.string, data.cmdargtoken[3].val.string, 1);
    else
        return 1;

    return(0);
}




void __attribute__ ((constructor)) libinit_img_reduce()
{
	if ( INITSTATUS_img_reduce == 0 )
	{
		init_img_reduce();
		RegisterModule(__FILE__, "milk", "Image analysis for astronomy: basic routines");
		INITSTATUS_img_reduce = 1;
	}
}


int_fast8_t init_img_reduce()
{
  strcpy(data.cmd[data.NBcmd].key,"rmbadpixfast");
  strcpy(data.cmd[data.NBcmd].module,__FILE__);
  data.cmd[data.NBcmd].fp = IMG_REDUCE_cleanbadpix_fast_cli;
  strcpy(data.cmd[data.NBcmd].info,"remove bad pixels (fast algo)");
  strcpy(data.cmd[data.NBcmd].syntax,"<image> <badpixmap> <output>");
  strcpy(data.cmd[data.NBcmd].example,"rmbadpixfast im bpmap outim");
  strcpy(data.cmd[data.NBcmd].Ccall,"long IMG_REDUCE_cleanbadpix_fast(const char *IDname, const char *IDbadpix_name, const char *IDoutname, int streamMode)");
  data.NBcmd++;

  strcpy(data.cmd[data.NBcmd].key,"rmbadpixfasts");
  strcpy(data.cmd[data.NBcmd].module,__FILE__);
  data.cmd[data.NBcmd].fp = IMG_REDUCE_cleanbadpix_stream_fast_cli;
  strcpy(data.cmd[data.NBcmd].info,"remove bad pixels (fast algo, stream)");
  strcpy(data.cmd[data.NBcmd].syntax,"<image> <badpixmap> <output>");
  strcpy(data.cmd[data.NBcmd].example,"rmbadpixfast imstream bpmap outimstream");
  strcpy(data.cmd[data.NBcmd].Ccall,"long IMG_REDUCE_cleanbadpix_fast(const char *IDname, const char *IDbadpix_name, const char *IDoutname, int streamMode)");
  data.NBcmd++;


  strcpy(data.cmd[data.NBcmd].key,"cubesimplestat");
  strcpy(data.cmd[data.NBcmd].module,__FILE__);
  data.cmd[data.NBcmd].fp = IMG_REDUCE_cubesimplestat_cli;
  strcpy(data.cmd[data.NBcmd].info,"simple data cube stats");
  strcpy(data.cmd[data.NBcmd].syntax,"<image>");
  strcpy(data.cmd[data.NBcmd].example,"cubesimplestat");
  strcpy(data.cmd[data.NBcmd].Ccall,"long IMG_REDUCUE_cubesimplestat(const char *IDin_name)");
  data.NBcmd++;

  strcpy(data.cmd[data.NBcmd].key,"imgcubeprocess");
  strcpy(data.cmd[data.NBcmd].module,__FILE__);
  data.cmd[data.NBcmd].fp = IMG_REDUCE_cubeprocess_cli;
  strcpy(data.cmd[data.NBcmd].info,"data cube process");
  strcpy(data.cmd[data.NBcmd].syntax,"<image>");
  strcpy(data.cmd[data.NBcmd].example,"imgcubeprocess");
  strcpy(data.cmd[data.NBcmd].Ccall,"int IMG_REDUCE_cubeprocess(const char *IDin_name)");
  data.NBcmd++;

 // add atexit functions here
  
  
  return 0;
}




/** compute ave, RMS
 *
 */


long IMG_REDUCE_cubesimplestat(const char *IDin_name)
{
    long IDin;
    long xsize, ysize, zsize;
    long xysize;
    long ii, kk;
    long offset;
    double tmpf;

    long IDave, IDrms;

    IDin = image_ID(IDin_name);

    xsize = data.image[IDin].md[0].size[0];
    ysize = data.image[IDin].md[0].size[1];
    zsize = data.image[IDin].md[0].size[2];

    xysize = xsize*ysize;


    IDave = create_2Dimage_ID("c_ave", xsize, ysize);
    IDrms = create_2Dimage_ID("c_rms", xsize, ysize);

    for(kk=0; kk<zsize; kk++)
    {
        offset = kk*xysize;
        for(ii=0; ii<xysize; ii++)
        {
            tmpf = data.image[IDin].array.F[offset+ii];
            data.image[IDave].array.F[ii] += tmpf;
            data.image[IDrms].array.F[ii] += tmpf*tmpf;
        }
    }

    for(ii=0; ii<xysize; ii++)
    {
        data.image[IDave].array.F[ii] /= zsize;
        data.image[IDrms].array.F[ii] /= zsize;
        data.image[IDrms].array.F[ii] = sqrt(data.image[IDrms].array.F[ii]-data.image[IDave].array.F[ii]*data.image[IDave].array.F[ii]);
    }

    return(IDin);
}






/// removes bad pixels in cube

int clean_bad_pix(const char *IDin_name, const char *IDbadpix_name)
{
    long ii, jj, kk;
    long IDin, IDbadpix, IDbadpix1; //, IDouttmp;
    long xsize, ysize, zsize;
    double *pix;
    double bpix[3][3];
    long i,j;
    double sum_bpix, *sum_pix;
    long left, fixed;
    long xysize;


    IDin = image_ID(IDin_name);

    xsize = data.image[IDin].md[0].size[0];
    ysize = data.image[IDin].md[0].size[1];
    zsize = data.image[IDin].md[0].size[2];
    xysize = xsize*ysize;

    sum_pix = (double*) malloc(sizeof(double)*zsize);
    pix = (double*) malloc(sizeof(double)*zsize*3*3);

    copy_image_ID(IDbadpix_name, "badpix_tmp", 0);
    IDbadpix = image_ID("badpix_tmp");
    copy_image_ID("badpix_tmp", "newbadpix_tmp", 0);
    IDbadpix1 = image_ID("newbadpix_tmp");

    //    copy_image_ID(IDin_name, "bpcleaned_tmp");
    //   IDouttmp = image_ID("bpcleaned_tmp");


    left = 1;
    while (left!=0)
    {
        left = 0;
        fixed = 0;

        for (jj = 1; jj < ysize-1; jj++)
            for (ii = 1; ii < xsize-1; ii++)
            {
                if (data.image[IDbadpix].array.F[jj*xsize+ii]>0.5)
                {
                    sum_bpix = 0.0;
                    for(kk=0; kk<zsize; kk++)
                        sum_pix[kk] = 0.0;

                    for (i = 0; i < 3; i++)
                        for (j = 0; j <3; j++)
                        {
                            for(kk=0; kk<zsize; kk++)
                                pix[kk*9+j*3+i] = data.image[IDin].array.F[kk*xysize+(jj-1+j)*xsize+(ii-1+i)];
                            bpix[i][j] = data.image[IDbadpix].array.F[(jj-1+j)*xsize+(ii-1+i)];
                            sum_bpix += bpix[i][j];
                            for(kk=0; kk<zsize; kk++)
                                sum_pix[kk] += (1.0-bpix[i][j])*pix[kk*9+j*3+i];

                        }
                    sum_bpix = 9.0-sum_bpix;
                    if (sum_bpix>2.1)
                    {
                        for(kk=0; kk<zsize; kk++)
                            data.image[IDin].array.F[kk*xysize+jj*xsize+ii] = sum_pix[kk]/sum_bpix;
                        data.image[IDbadpix1].array.F[jj*xsize+ii] = 0.0;
                        fixed += 1;
                    }
                    else
                        left += 1;
                }
            }

        for (jj = 1; jj < ysize-1; jj++)
            for (ii = 1; ii < xsize-1; ii++)
                data.image[IDbadpix].array.F[jj*xsize+ii] = data.image[IDbadpix1].array.F[jj*xsize+ii];

        printf(" %ld bad pixels cleaned. %ld pixels left\n",fixed,left);
    }
    delete_image_ID("badpix_tmp");
    delete_image_ID("newbadpix_tmp");

    free(sum_pix);
    free(pix);

    return(0);
}





// pre-compute operations to clean bad pixels
long IMG_REDUCE_cleanbadpix_fast_precompute(const char *IDmask_name)
{
    long NBop;
    long IDbadpix;
    int left;
    long NBopmax;
    long xsize, ysize;
    long xysize;
    long ii, jj;
    long ii1, jj1;
    long k;
    long distmax;
    long NBnearbypix;
    long NBnearbypix_max;
    long bpcnt;

    long *nearbypix_array_index;
    float *nearbypix_array_dist2;
    float *nearbypix_array_coeff;

    float coefftot;

    printf("Pre-computing bad pixel compensation operations\n");
    fflush(stdout);

    IDbadpix = image_ID(IDmask_name);
    xsize = data.image[IDbadpix].md[0].size[0];
    ysize = data.image[IDbadpix].md[0].size[1];

    xysize = xsize*ysize;
    NBopmax = xysize*100;


    nearbypix_array_index = (long*) malloc(sizeof(long)*xysize);
    nearbypix_array_dist2 = (float*) malloc(sizeof(float)*xysize);
    nearbypix_array_coeff = (float*) malloc(sizeof(float)*xysize);

    badpixclean_init = 1;

    badpixclean_indexlist = (long*) malloc(sizeof(long)*xysize);
    k = 0;
    for(ii=0; ii<xysize; ii++)
    {
        if (data.image[IDbadpix].array.F[ii]>0.5)
        {
            badpixclean_indexlist[k] = ii;
            k++;
        }
    }

    badpixclean_NBbadpix = k;



    badpixclean_array_indexin = (long*) malloc(sizeof(long)*xysize);
    badpixclean_array_indexout = (long*) malloc(sizeof(long)*xysize);
    badpixclean_array_coeff = (float*) malloc(sizeof(float)*xysize);


    printf("Computing operations...\n");
    fflush(stdout);
    NBop = 0;
    bpcnt = 0;

    for(ii=0; ii<xsize; ii++)
        for(jj=0; jj<ysize; jj++)
        {
            if (data.image[IDbadpix].array.F[jj*xsize+ii]>0.5)
            {
                // fill up array of nearby pixels
                k = 0;
                distmax = 1;
                while(k<4)
                {
                    k = 0;
                    coefftot = 0.0;
                    for(ii1=ii-distmax; ii1<ii+distmax+1; ii1++)
                        for(jj1=jj-distmax; jj1<jj+distmax+1; jj1++)
                        {
                            if((ii1>-1)&&(ii1<xsize)&&(jj1>-1)&&(jj1<ysize)&&(data.image[IDbadpix].array.F[jj1*xsize+ii1]<0.5))
                            {
                                if((ii1!=ii)||(jj1!=jj))
                                {
                                    nearbypix_array_index[k] = (long) (jj1*xsize+ii1);
                                    nearbypix_array_dist2[k] = (float) (1.0*(ii1-ii)*(ii1-ii)+1.0*(jj1-jj)*(jj1-jj));
                                    nearbypix_array_coeff[k] = pow(1.0/nearbypix_array_dist2[k],2.0);
                                    coefftot += nearbypix_array_coeff[k];
                                    k++;
                                    if(k>xysize-1)
                                    {
                                        printf("ERROR: too many nearby pixels\n");
                                        exit(0);
                                    }
                                }
                            }
                        }
                    distmax++;
                }
                NBnearbypix = k;
                //      printf("%ld  distmax = %ld  -> k = %ld / %ld\n", bpcnt, distmax, NBnearbypix, xysize);
                //    fflush(stdout);

                if(NBnearbypix>xysize)
                {
                    printf("ERROR: NBnearbypix>xysize\n");
                    exit(0);
                }


                for(k=0; k<NBnearbypix; k++)
                {
                    nearbypix_array_coeff[k] /= coefftot;


                    badpixclean_array_indexin[NBop] = nearbypix_array_index[k];
                    badpixclean_array_indexout[NBop] = jj*xsize+ii;
                    badpixclean_array_coeff[NBop] = nearbypix_array_coeff[k];
                    NBop++;
                
                    if(NBop>xysize-1)
                    {
                        printf("ERROR: TOO MANY BAD PIXELS .... sorry... you need a better detector.\n");
                        exit(0);
                    }
                }


                bpcnt++;
            }
        }
    printf("%ld bad pixels\n", bpcnt);
    printf("%ld / %ld  operations\n", NBop, xysize);


    printf("free nearbypix_array_index ...\n");
    fflush(stdout);
    free(nearbypix_array_index);

    printf("free nearbypix_array_dist2 ...\n");
    fflush(stdout);
    free(nearbypix_array_dist2);

    printf("free nearbypix_array_coeff ...\n");
    fflush(stdout);
    free(nearbypix_array_coeff);

    return(NBop);
}








long IMG_REDUCE_cleanbadpix_fast(const char *IDname, const char *IDbadpix_name, const char *IDoutname, int streamMode)
{
    long ID;
    uint32_t *sizearray;
    long k;
    long xysize, zsize;
    long IDout;
    long IDdark;
    long ii, kk;
    int naxis;


    ID = image_ID(IDname);
    sizearray = (uint32_t*) malloc(sizeof(uint32_t)*3);
    sizearray[0] = data.image[ID].md[0].size[0];
    sizearray[1] = data.image[ID].md[0].size[1];
    naxis = 2;
    if(data.image[ID].md[0].naxis == 3)
	{	
		sizearray[2] = data.image[ID].md[0].size[2];
		zsize = sizearray[2];
		naxis = 3;
	}
	else
		zsize = 1;
    
    
    xysize = sizearray[0]*sizearray[1];

    IDdark = image_ID("dark"); // use if it exists
    list_image_ID();

    IDout = image_ID(IDoutname);
    if(IDout==-1)
    {
        printf("Creating output image\n");
        fflush(stdout);
        if(streamMode==1)
        {
			IDout = create_image_ID(IDoutname, naxis, sizearray, _DATATYPE_FLOAT, 1, 0);
			COREMOD_MEMORY_image_set_createsem(IDoutname, 2);
		}
		else
		{
			IDout = create_image_ID(IDoutname, naxis, sizearray, _DATATYPE_FLOAT, 0, 0);
		}
    }
    if(streamMode == 1)
		COREMOD_MEMORY_image_set_createsem(IDoutname, 2);

    if(badpixclean_init==0)
        badpixclean_NBop = IMG_REDUCE_cleanbadpix_fast_precompute(IDbadpix_name);


	int OKloop = 1;
    while(OKloop==1)
    {
		if(streamMode==1)
		{
        printf("Waiting for incoming image ... \n");
        fflush(stdout);
        if(data.image[ID].md[0].sem>0)
            sem_wait(data.image[ID].semptr[0]);
        else
        {
            printf("NO SEMAPHORE !!!\n");
            fflush(stdout);
            exit(0);
        }
		}
		else
			OKloop = 0;
     
        data.image[IDout].md[0].write = 1;
        
        
         memcpy(data.image[IDout].array.F, data.image[ID].array.F, sizeof(float)*xysize*zsize); 
        
        for(kk=0;kk<zsize;kk++)
        { 
             if(IDdark!=-1)
				for(ii=0; ii<xysize; ii++)
					data.image[IDout].array.F[kk*xysize+ii] -= data.image[IDdark].array.F[ii];

			for(k=0; k<badpixclean_NBbadpix; k++)
				data.image[IDout].array.F[kk*xysize+badpixclean_indexlist[k]] = 0.0;



			for(k=0; k<badpixclean_NBop; k++)
			{
			//    printf("Operation %ld / %ld    %ld x %f -> %ld", k, badpixclean_NBop, badpixclean_array_indexin[k], badpixclean_array_coeff[k], badpixclean_array_indexout[k]);
			//   fflush(stdout);
				data.image[IDout].array.F[kk*xysize+badpixclean_array_indexout[k]] += badpixclean_array_coeff[k]*data.image[IDout].array.F[kk*xysize+badpixclean_array_indexin[k]];
			//  printf("\n");
			//  fflush(stdout);
			}
		}
		
		if(streamMode==1)
		{
			if(data.image[IDout].md[0].sem > 0)
				sem_post(data.image[IDout].semptr[0]);
		}
        data.image[IDout].md[0].write = 0;
        data.image[IDout].md[0].cnt0++;
    }

    free(sizearray);

    return(IDout);
}






int IMG_REDUCE_correlMatrix(const char *IDin_name,  const char *IDmask_name, const char *IDout_name)
{
	long IDin, IDout;
	long IDmask;
	long xsize, ysize, zsize;
    long xysize;
    long ii, jj, kk, kk1, kk2;
	double v, tot;
	double tot1, tot2;
	
	IDin = image_ID(IDin_name);

    xsize = data.image[IDin].md[0].size[0];
    ysize = data.image[IDin].md[0].size[1];
    zsize = data.image[IDin].md[0].size[2];

    xysize = xsize*ysize;

	IDmask = image_ID(IDmask_name);
	IDout = create_2Dimage_ID(IDout_name, zsize, zsize);

	for(kk1=0;kk1<zsize;kk1++)
		for(kk2=kk1+1;kk2<zsize;kk2++)
		{
			tot = 0.0;
			tot1 = 0.0;
			tot2 = 0.0;
			
			
			for(ii=0;ii<xysize;ii++) 
				{
					tot1 += data.image[IDin].array.F[kk1*xysize+ii];
					tot2 += data.image[IDin].array.F[kk2*xysize+ii];
				}
			
			
			for(ii=0;ii<xysize;ii++) 
				{
					v = (data.image[IDin].array.F[kk1*xysize+ii]/tot1) - (data.image[IDin].array.F[kk2*xysize+ii]/tot2);
					tot += v*v*data.image[IDmask].array.F[ii];
				}
			data.image[IDout].array.F[kk2*zsize+kk1] = tot;
		}	
	
	return(0);
}






/** this is the main routine to pre-process a cube stream of images (PSFs)
 * 
 * 
 * Optional inputs:
 * 	calib_darkim  (single frame or cube)
 *  calib_badpix (single frame)
 *  calib_flat 
 * 
 * 
 */

int IMG_REDUCE_cubeprocess(const char *IDin_name)
{
    long IDin;
    long xsize, ysize, zsize;
    long xysize;
    long ii, jj, kk;

    long IDflat;
    long IDbadpix;

    long IDdark;
    long xsized, ysized, zsized;
    long xysized;
    long kk1;

    double *xcent;
    double *ycent;
    double xtot, ytot, tot;
    double boxrad;
    double v;
    long xmin, xmax, ymin, ymax;

    long xsize1, ysize1, xysize1; // crop size
    long ID1, ID2;
    long ii1, jj1;
	long IDt1, IDt2;

	double x, y, r;
	long ID;

	long kk2;
	long kk1min, kk2min;
	double vmin;
	long IDdiff;
	

    IDin = image_ID(IDin_name);

    xsize = data.image[IDin].md[0].size[0];
    ysize = data.image[IDin].md[0].size[1];
    zsize = data.image[IDin].md[0].size[2];

    xysize = xsize*ysize;

    /// remove dark
    if((IDdark=image_ID("calib_dark"))!=-1)
    {
        printf("REMOVING DARK ...");
        fflush(stdout);
        xsized = data.image[IDdark].md[0].size[0];
        ysized = data.image[IDdark].md[0].size[1];
        zsized = data.image[IDdark].md[0].size[2];
        xysized = xsized*ysized;

        list_image_ID();
        kk1 = 0;
        for(kk=0; kk<zsize; kk++)
        {
            for(ii=0; ii<xysize; ii++)
                data.image[IDin].array.F[kk*xysize+ii] -= data.image[IDdark].array.F[kk1*xysize+ii];
            kk1 ++;
            if(kk1==zsized)
                kk1 = 0;
        }
        printf(" DONE\n");
        fflush(stdout);

    }

    /// remove bad pixels
    if(image_ID("calib_badpix")!=-1)
    {
        printf("REMOVING BAD PIXELS ...");
        fflush(stdout);
        clean_bad_pix(IDin_name, "calib_badpix");
        save_fits(IDin_name, "!out1.fits");
        list_image_ID();
    }


    /// compute photocenter
    xcent = (double*) malloc(sizeof(double)*zsize);
    ycent = (double*) malloc(sizeof(double)*zsize);

    for(kk=0; kk<zsize; kk++)
    {
        xtot = 0.0;
        ytot = 0.0;
        tot = 0.0;

        xcent[kk] = 0.5*xsize;
        ycent[kk] = 0.5*ysize;

        boxrad = 50.0;

        xmin = (long) (xcent[kk]-boxrad);
        xmax = (long) (xcent[kk]+boxrad);

        ymin = (long) (ycent[kk]-boxrad);
        ymax = (long) (ycent[kk]+boxrad);

        if(xmin<0)
            xmin = 0;
        if(xmax>xsize-1)
            xmax = xsize-1;

        if(ymin<0)
            ymin = 0;
        if(ymax>ysize-1)
            ymax = ysize-1;

        //printf(" %4ld %4ld    %4ld %4ld\n", xmin, xmax, ymin, ymax);
        //fflush(stdout);

        for(ii=xmin; ii<xmax; ii++)
            for(jj=ymin; jj<ymax; jj++)
            {
                v = data.image[IDin].array.F[kk*xysize+jj*xsize+ii];
                xtot += v*ii;
                ytot += v*jj;
                tot += v;
            }
        xcent[kk] = xtot/tot;
        ycent[kk] = ytot/tot;


        printf("%6ld   %12lf   %12lf\n", kk, xcent[kk], ycent[kk]);
        fflush(stdout);
    }

    xsize1 = 128;
    ysize1 = 128;
    xysize1 = xsize1*ysize1;

    ID1 = create_3Dimage_ID("cropPSF", xsize1, ysize1, zsize);

    for(kk=0; kk<zsize; kk++)
        for(ii1=0; ii1<xsize1; ii1++)
            for(jj1=0; jj1<xsize1; jj1++)
            {
                ii = ii1 - xsize1/2 + (long) (xcent[kk]+0.5);
                jj = jj1 - ysize1/2 + (long) (ycent[kk]+0.5);
                if((ii>-1)&&(ii<xsize)&&(jj>-1)&&(jj<ysize))
                    data.image[ID1].array.F[kk*xysize1+jj1*xsize1+ii1] = data.image[IDin].array.F[kk*xysize+jj*xsize+ii];
            }






	IDt1 = create_2Dimage_ID("translin", xsize1, ysize1);

    for(kk=0; kk<zsize; kk++)
    {
        xtot = 0.0;
        ytot = 0.0;
        tot = 0.0;

        xcent[kk] = 0.5*xsize;
        ycent[kk] = 0.5*ysize;

        xtot = 0.0;
        ytot = 0.0;
        tot = 0.0;

        for(ii1=0; ii1<xsize1; ii1++)
            for(jj1=0; jj1<ysize1; jj1++)
            {
                v = data.image[ID1].array.F[kk*xysize1+jj1*xsize1+ii1];
                xtot += v*ii1;
                ytot += v*jj1;
                tot += v;
				data.image[IDt1].array.F[jj1*xsize1+ii1] = v;
            }
		xcent[kk] = xtot/tot;
        ycent[kk] = ytot/tot;

		printf("%6ld   %12lf   %12lf\n", kk, xcent[kk], ycent[kk]);

        fft_image_translate("translin", "translout", xcent[kk]-0.5*xsize1, ycent[kk]-0.5*ysize1);
		IDt2 = image_ID("translout");
		
		for(ii=0;ii<xysize1;ii++)
			data.image[ID1].array.F[kk*xysize1+ii] = data.image[IDt2].array.F[ii];
		
		delete_image_ID("translout");
    }




    free(xcent);
    free(ycent);





    save_fits("cropPSF", "!cropPSF.fits");

	ID = create_2Dimage_ID("corrmask", xsize1, ysize1);
	for(ii1=0; ii1<xsize1; ii1++)
        for(jj1=0; jj1<ysize1; jj1++)
			{
				x = 1.0*ii1 - 0.5*xsize1;
				y = 1.0*jj1 - 0.5*ysize1;
				r = sqrt(x*x+y*y);
				if((x<0.0)&&(r>15.0))
					data.image[ID].array.F[jj1*xsize1+ii1] = 1.0;
				else
					data.image[ID].array.F[jj1*xsize1+ii1] = 0.0;
				if((fabs(y)>30.0)||(x<-30)||(y<0.0))
					data.image[ID].array.F[jj1*xsize1+ii1] = 0.0;
			}
	
	save_fits("corrmask", "!corrmask.fits");

	IMG_REDUCE_correlMatrix("cropPSF", "corrmask", "cropPSF_corr");
	save_fits("cropPSF_corr", "!cropPSF_corr.fits");
	
	ID = image_ID("cropPSF_corr");
	kk1 = 0;
	kk2 = 500;
	vmin = data.image[ID].array.F[kk2*zsize+kk1]*2.0;
	for(kk1=0;kk1<zsize;kk1++)
		for(kk2=kk1+100;kk2<zsize;kk2++)
			{
				if(data.image[ID].array.F[kk2*zsize+kk1]<vmin)
					{
						vmin = data.image[ID].array.F[kk2*zsize+kk1];
						kk1min = kk1;
						kk2min = kk2;
					}				
			}
	
	printf("MOST SIMILAR PAIR : %ld %ld   [%lf]\n", kk1min, kk2min, vmin);

	ID1 = create_2Dimage_ID("imp1", xsize1, ysize1);
	ID2 = create_2Dimage_ID("imp2", xsize1, ysize1);
	IDdiff = create_2Dimage_ID("imdiff", xsize1, ysize1);
	ID = image_ID("cropPSF");
	
	list_image_ID();
	
	for(ii=0;ii<xysize1;ii++)
		{
			data.image[ID1].array.F[ii] = data.image[ID].array.F[kk1min*xysize1+ii];
			data.image[ID2].array.F[ii] = data.image[ID].array.F[kk2min*xysize1+ii];
			data.image[IDdiff].array.F[ii] = data.image[ID].array.F[kk1min*xysize1+ii] - data.image[ID].array.F[kk2min*xysize1+ii];
		}
	
	save_fits("imp1", "!imp1.fits");
	save_fits("imp2", "!imp2.fits");
	save_fits("imdiff", "!impdiff.fits");

    return(0);
}





