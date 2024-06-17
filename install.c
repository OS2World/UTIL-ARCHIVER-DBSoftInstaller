/* 
 *  install.c (c) 1998,1999 Brian Smith
 *  parts by Daniele Vistalli
 */

#define INCL_WIN       /* Window Manager Functions     */
#define INCL_DOS
#define INCL_BASE
#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>      
#include <sys/types.h>
#include "install.h"

HAB hab = 0;
HMQ hmq = 0;
HPS hps = 0;
HDC hdc = 0;
HBITMAP hbm = 0;
POINTL bmppoint;
SIZEL sizl;
QMSG	qmsg = { 0 };			 
HWND mainhwnd = 0, logohwnd = 0;
int installstate = NONE;
int installstage = 0;
char installdir[400];
int current_file=0, success=0;
unsigned long int acepos=0, aceoffset=0;
int pixels=0;
char confirmstring[1024];
char *configsys[4098];
int configsyscount=-1;
int files = 0, files_deleted=0, packagesize=0, packagesselected[20];
int driveselected, packagechosen;
FILE *self;

/* These get loaded in loadheader */
char *INSTALLER_APPLICATION;
char *INSTALLER_VERSION;
char *INSTALLER_TITLE;
char *INSTALLER_PATH;
char *INSTALLER_FOLDER;
char *INSTALLER_PROGRAM;
char *INSTALLER_SHADOW;
char *INSTALLER_SETS;
char *INSTALLER_SYSVAR;
char *INSTALLER_PACKAGES[20];
int INSTALLER_BITMAP_WIDTH;
int INSTALLER_BITMAP_HEIGHT;
int INSTALLER_PACKAGE_COUNT;

/* Config.Sys -- Note the drive letter gets replaced with the boot drive letter
                 It is just a place holder. (For the next 3 entries)             */
char csfile[] = "C:\\CONFIG.SYS";
/* Backup Config.Sys filename */
char bufile[] = "C:\\CONFIG.NST";
/* Installation Log Database -- Used for uninstalltion and aborting */
char instlog[] = "C:\\OS2\\INSTALL\\DBINST.LOG";
FILE *logfile;

/* Function prototypes */
int installer_unpack(CHAR * filename, int operation);
int MakeShadow(char Title[], char reference[], char dest[]);
int MakeProgram(char Title[], char Program[], char Icon[], char dest[], char id[], char setup[]);
int MakeFolder(char Title[], char Icon[], char dest[], char id[], char setup[]);
void resetglobals(void);

int mesg(char *format, ...) {
  va_list args;
  char outbuf[256];

  va_start(args, format);
  vsprintf(outbuf, format, args);
  va_end(args);

  WinMessageBox(HWND_DESKTOP, HWND_DESKTOP, outbuf, INSTALLER_TITLE, 0, MB_OK | MB_INFORMATION | MB_MOVEABLE);

  return strlen(outbuf);
}

int checktext(char *text, char *buffer, int buflen)
{
    int z, len = strlen(text);
    
    for(z=0;z<(buflen-len);z++)
    {
        if(memcmp(text, &buffer[z], len) == 0)
            return z;
    }
    return -1;

}
long findtext(char *text)
{
    char buffer[512];
    int offset;
    unsigned long curpos = 0;

    fseek(self, 0, SEEK_SET);
    fread(buffer, 1, 512, self);
    if((offset = checktext(text, buffer, 512)) > -1)
        return offset;
    while(!feof(self))
    {
        memcpy(buffer, &buffer[256], 256);
        fread(&buffer[256], 1, 256, self);
        curpos += 256;
        if((offset = checktext(text, buffer, 512)) > -1)
            return offset+curpos;
        
    }
    return -1;
}
    
int aceseek_entry(int num)
{
    unsigned long headerstart;
    char headerbuf[20], packageconfig[100], cur;
    int z, start = 0, entry = 0, packageoffset=0;

    sprintf(headerbuf, "DBSOFT-ACE%d", num);
    if((headerstart = findtext(headerbuf)) < 0)
    {
        mesg("Could not find ACE header in executable.");
        exit(2);
    }

    fseek(self, headerstart+strlen(headerbuf), SEEK_SET);
    fread(packageconfig, 1, 100, self);
    
    for(z=0;z<100;z++)
    {
        if(packageconfig[z] == '-' || packageconfig[z] == ';')
        {
            char cur = packageconfig[z];
            packageconfig[z] = 0;
            switch(entry)
            {
                case 0:
                    current_file = 0;
                    files = atoi(&packageconfig[start]);
                    break;
                case 1:
                    packagesize = atoi(&packageconfig[start]);
                    break;
            }
            start = z+1;
            if(cur == ';')
            {
                packageoffset = z + 1;
                z = 100;
            }
            entry++;
        }
    }
    aceoffset=headerstart+strlen(headerbuf)+packageoffset;
    fseek(self, aceoffset, SEEK_SET);
    return TRUE;
}

int loadheader(void)
{
    char *buffer;
    long headerstart;
    int z, pos=0, start=0, entry=0, done=0;

    buffer = malloc(8096*4);
    
    if((headerstart = findtext("DBSOFT-HEADER")) < 0)
    {
        mesg("Could not find Selfinstaller header in executable.");
        exit(2);
    }
    //mesg("headerstart %ld", headerstart);
    fseek(self, headerstart+13, SEEK_SET);
    
    fread(buffer, 1, 8096*4, self);
    for(z=0;z<8096*4;z++)
    {
        if(buffer[z] == '-' || buffer[z] == ';')
        {
            char cur = buffer[z];
            buffer[z] = 0;
            //mesg("z %d entry %d string \"%s\"", z, entry, &buffer[start]);
            switch(entry)
            {
                case 0:
                    INSTALLER_APPLICATION = (char *)strdup(&buffer[start]);
                    break;
                case 1:
                    INSTALLER_VERSION = (char *)strdup(&buffer[start]);
                    break;
                case 2:
                    INSTALLER_TITLE = (char *)strdup(&buffer[start]);
                    break;
                case 3:
                    INSTALLER_PATH = (char *)strdup(&buffer[start]);
                    break;
                case 4:
                    INSTALLER_FOLDER = (char *)strdup(&buffer[start]);
                    break;
                case 5:
                    INSTALLER_PROGRAM = (char *)strdup(&buffer[start]);
                    break;
                case 6:
                    INSTALLER_SHADOW = (char *)strdup(&buffer[start]);
                    break;
                case 7:
                    INSTALLER_SETS = (char *)strdup(&buffer[start]);
                    break;
                case 8:
                    INSTALLER_SYSVAR = (char *)strdup(&buffer[start]);
                    break;
                case 9:
                    INSTALLER_BITMAP_WIDTH = atoi(&buffer[start]);
                    break;
                case 10:
                    INSTALLER_BITMAP_HEIGHT = atoi(&buffer[start]);
                    break;
                case 11:
                    INSTALLER_PACKAGE_COUNT = atoi(&buffer[start]);
                    break;
                default:
                    INSTALLER_PACKAGES[entry-12] = malloc((z-start)+1);
                    strcpy(INSTALLER_PACKAGES[entry-12], &buffer[start]);
                    break;
            }
            start = z+1;
            if(cur == ';')
            {
                free(buffer);
                return TRUE;
            }
            
            entry++;
        }
    }
    free(buffer);
    return FALSE;
}

int aceread(void *buf, size_t count)
{
    unsigned long curpos = ftell(self);
    size_t readit;
    
    if(count >  (packagesize-(curpos-aceoffset)))
        readit = (packagesize-(curpos-aceoffset));
    else
        readit = count;
    
   return fread(buf, 1, readit, self);
}

off_t acelseek(off_t offset, int whence)
{
  switch(whence)
  {
  case SEEK_SET:
      fseek(self, aceoffset+offset, SEEK_SET);
      break;
  case SEEK_CUR:
      fseek(self, offset, SEEK_CUR);
     break;
  }
  return acepos;
}

int aceopen(const char *path, int flags)
{
    fseek(self, aceoffset, SEEK_SET);
    return 0;
}

int aceclose(int fd)
{
    fseek(self, aceoffset, SEEK_SET);
    return  0;
}

void delete_files(void)
{
   char tmpbuf[8196], *fileptr;
   FILE *tmplf;
   int linenum=0, found=-1, z;

   files_deleted=1;

   if((tmplf=fopen(instlog, "rb"))==NULL)
      return;

   while(!feof(tmplf))
   {
   fgets(tmpbuf, 8196, tmplf);
   linenum++;
   if(tmpbuf[0]=='[' && (char *)strstr(tmpbuf, INSTALLER_APPLICATION) != NULL && !feof(tmplf))
      {
        fgets(tmpbuf, 8196, tmplf);
        linenum++;
        if((char *)strstr(tmpbuf, "<Version>") != NULL && (char *)strstr(tmpbuf, INSTALLER_VERSION) != NULL)
           found=linenum;
      }
   }
   if(found != -1)
      {
         rewind(tmplf);
         for (z=0;z<found;z++) 
            fgets(tmpbuf, 8196, tmplf);
         while(!feof(tmplf))
            {
            fgets(tmpbuf, 8196, tmplf);
            if((char *)strstr(tmpbuf, "<FileInst>") != NULL)  
               {
                  fileptr = (char *)strchr(tmpbuf, ',')+1;
                  /* Remove trailing CRLFs */
                  if(fileptr[strlen(fileptr)-1] == '\r' || fileptr[strlen(fileptr)-1] == '\n')
                     fileptr[strlen(fileptr)-1]=0;
                  if(fileptr[strlen(fileptr)-1] == '\r' || fileptr[strlen(fileptr)-1] == '\n')
                     fileptr[strlen(fileptr)-1]=0;
                  unlink(fileptr);
                  current_file--;
                  WinSendMsg(mainhwnd, WM_USER, 0, 0);
               }
            if((char *)strstr(tmpbuf, "<End>") != NULL)
               {
                  fclose(tmplf);
                  return;
               }
            }
      }
   fclose(tmplf);
   return;
}

int readconfigsys(void)
{
   char tmpbuf[8196];
   FILE *tmpcs;

   if((tmpcs=fopen(csfile, "rb"))==NULL)
      return 1;

   while(!feof(tmpcs))
   {
   configsyscount++;
   fgets(tmpbuf, 8196, tmpcs);
   configsys[configsyscount] = malloc(strlen(tmpbuf)+1);
   strcpy(configsys[configsyscount], tmpbuf);
   }

   fclose(tmpcs);
   return 0;
}


int writeconfigsys(void)
{
   FILE *tmpcs;
   int i;

   unlink(bufile);

   DosMove(csfile, bufile);

   if((tmpcs=fopen(csfile, "wb"))==NULL)
      return 1;

   for(i=0;i<configsyscount;i++)
      {
      if(configsys[i])
         {
         fwrite(configsys[i], 1, strlen(configsys[i]), tmpcs);
         free(configsys[i]);
         }
      }

   fclose(tmpcs);
}

void updateset(char *setname, char *newvalue, int flag)
{
   char *cmpbuf1, *cmpbuf2, *tmpptr, *tmpptr2, *nv;
   int i, z, t;

   if(stricmp(newvalue, "%INSTALLPATH%")==0)
      nv=installdir;
   else
      nv=newvalue;

   cmpbuf1=malloc(strlen(setname)+2);
   strcpy(cmpbuf1, setname);
   strcat(cmpbuf1, "=");
   for(i=0;i<configsyscount;i++)
      {
       if(strlen(cmpbuf1) <= strlen(configsys[i]))
          {
          tmpptr=malloc(strlen(configsys[i])+1);
          strcpy(tmpptr,configsys[i]);
          strupr(tmpptr);
          if((tmpptr2=(char*)strstr(tmpptr, "SET "))!=NULL)
           {
           tmpptr2 += 4;
           cmpbuf2=malloc(strlen(tmpptr2)+1);
           /* Remove any spaces from the string */
           z=0;
           for (t=0;t<strlen(tmpptr2) && z < strlen(cmpbuf1);t++) 
              {
                 if(tmpptr2[t] != ' ')
                    {
                      cmpbuf2[z]=tmpptr2[t];
                      z++;
                    }
              }
           cmpbuf2[z]=0;
           if(stricmp(cmpbuf1, cmpbuf2) == 0)
              {
               /* Ok we found the entry, and if UPDATE_ALWAYS change it to the
                  new entry, otherwise exit */
               if(flag == UPDATE_ALWAYS)
                  {
                  fprintf(logfile, "<CSRemLine>,%s", configsys[i]);
                  free(configsys[i]);
                  configsys[i] = malloc(strlen(cmpbuf1)+strlen(nv)+3);
                  strcpy(configsys[i], cmpbuf1);
                  strcat(configsys[i], nv);
                  strcat(configsys[configsyscount], "\r\n");
                  fprintf(logfile, "<CSAddLine>,%s", configsys[i]);
                  free(cmpbuf1);free(cmpbuf2);free(tmpptr);
                  }
               return;
              }
           free(cmpbuf2);
           }
          free(tmpptr);
          }
      }
   /* Couldn't find the line so we'll add it */
   configsys[configsyscount]=malloc(strlen(cmpbuf1)+strlen(nv)+7);
   strcpy(configsys[configsyscount], "SET ");
   strcat(configsys[configsyscount], cmpbuf1);
   strcat(configsys[configsyscount], nv);
   strcat(configsys[configsyscount], "\r\n");
   fprintf(logfile, "<CSAddLine>,%s", configsys[configsyscount]);
   configsyscount++;
   free(cmpbuf1);
}     

/* In str1, str2 gets replaced by str3 */
char *replacestr(char *str1, char *str2, char *str3)
{
    char bigbuffer[4096];
    int z, x=0, len1 = strlen(str1), len2 = strlen(str2), len3 = strlen(str3);

    for(z=0;z<len1;z++)
    {
        if(len2 > 0 && strncmp(&str1[z], str2, len2)==0)
        {
            int i;
            for(i=0;i<len3;i++)
            {
                bigbuffer[x] = str3[i];
                x++;
            }
            z=z+(len2-1);
        } else {
            bigbuffer[x] = str1[z];
            x++;
        }
    }
    bigbuffer[x] = 0;
    return (char *)strdup(bigbuffer);
}

void updatesys(char *sysname, char *newvalue)
{
   char *cmpbuf1, *cmpbuf2, *tmpptr, *tmpptr2, *capbuf1, *capbuf2, *nv, *brian;
   int i, z, t;

   nv=replacestr(newvalue, "%INSTALLPATH%", installdir);

   cmpbuf1=malloc(strlen(sysname)+2);
   strcpy(cmpbuf1, sysname);
   strcat(cmpbuf1, "=");
   for(i=0;i<configsyscount;i++)
      {
       if(strlen(cmpbuf1) <= strlen(configsys[i]))
          {
          cmpbuf2=malloc(strlen(configsys[i])+1);
          /* Remove any spaces from the string */
          z=0;
          for (t=0;t<strlen(configsys[i]) && z < strlen(cmpbuf1);t++) 
             {
               if(configsys[i][t] != ' ')
                  {
                     cmpbuf2[z]=configsys[i][t];
                     z++;
                  }
             }
          cmpbuf2[z]=0;
          if(stricmp(cmpbuf1, cmpbuf2) == 0)
             {
                /* Do a case insensitive comparison but preserve the case */
              tmpptr = &configsys[i][t];
              capbuf1=malloc(strlen(tmpptr)+1);
              capbuf2=malloc(strlen(nv)+1);
              strcpy(capbuf1, tmpptr);
              strcpy(capbuf2, nv);
              strupr(capbuf1);
              strupr(capbuf2);
              /* Ok, we found the line, and it doesn't have an entry so we'll add it */
              if((tmpptr2=(char *)strstr(capbuf1, capbuf2)) == NULL)
                 {
                 fprintf(logfile, "<CSRemLine>,%s", configsys[i]);
                 brian = configsys[i];
                 configsys[i] = malloc(strlen(configsys[i])+strlen(nv)+6);
                 strcpy(configsys[i], brian);
                 free(brian);
                 /* Remove any trailing CRLFs */
                 if(configsys[i][strlen(configsys[i])-1]=='\r' || configsys[i][strlen(configsys[i])-1]=='\n')
                       configsys[i][strlen(configsys[i])-1]=0;
                 if(configsys[i][strlen(configsys[i])-1]=='\r' || configsys[i][strlen(configsys[i])-1]=='\n')
                       configsys[i][strlen(configsys[i])-1]=0;
                 if(configsys[i][strlen(configsys[i])-1]!=';')
                       strcat(configsys[i], ";");
                 strcat(configsys[i], nv);
                 strcat(configsys[i], ";\r\n");
                 fprintf(logfile, "<CSAddLine>,%s", configsys[i]);
                 }
              free(cmpbuf1);free(cmpbuf2);free(capbuf1);free(capbuf2);
              return;
             }
          free(cmpbuf2);
          }
      }
   /* Couldn't find the line so we'll add it */
   configsys[configsyscount]=malloc(strlen(cmpbuf1)+strlen(nv)+5);
   strcpy(configsys[configsyscount], cmpbuf1);
   strcat(configsys[configsyscount], nv);
   strcat(configsys[configsyscount], ";\r\n");
   fprintf(logfile, "<CSAddLine>,%s", configsys[configsyscount]);
   configsyscount++;
   free(cmpbuf1);
   if(nv)
       free(nv);
}     

MRESULT	EXPENTRY DestDlgProc(HWND hWnd, ULONG msg, MPARAM mp1,	MPARAM mp2)
{
CHAR szBuffer[512];		   /* String Buffer			*/
SWP winpos;

switch ( msg )
   {
   case	WM_INITDLG :
        WinSetWindowText(hWnd, INSTALLER_TITLE);
        WinSetDlgItemText(hWnd,	I_Text, installdir);
        WinQueryWindowPos(mainhwnd, &winpos);
        WinSetWindowPos(hWnd, HWND_TOP, winpos.x+30, winpos.y+30, 0, 0, SWP_MOVE | SWP_ZORDER);
	break;
			/* Process push	button selections		*/
   case	WM_COMMAND :
	switch ( SHORT1FROMMP(mp1) )
	    {
	    case I_Cancel2:
  	       WinDismissDlg(hWnd, FALSE);
               break;
            case I_OK:
               WinQueryDlgItemText(hWnd, I_Text,	399L, installdir);
  	       WinDismissDlg(hWnd, FALSE);
               break;
	    }
	break;
			/* Close requested, exit dialogue		*/
   case	WM_CLOSE :
	WinDismissDlg(hWnd, FALSE);
	break;

			/* Pass	through	unhandled messages		*/
   default :
       return(WinDefDlgProc(hWnd, msg, mp1, mp2));
   }
return(0L);
}

unsigned long long drivefree(int drive)
{
    ULONG   aulFSInfoBuf[40] = {0};
    APIRET  rc               = NO_ERROR;
 
    DosError(FERR_DISABLEHARDERR);
    rc = DosQueryFSInfo(drive,
                        FSIL_ALLOC,
                        (PVOID)aulFSInfoBuf,
                        sizeof(aulFSInfoBuf));
 
    DosError(FERR_ENABLEHARDERR);
    if (rc != NO_ERROR)
       return 0;

    return aulFSInfoBuf[3] * aulFSInfoBuf[1] * (USHORT)aulFSInfoBuf[4];
}
 
       /* (Sectors per allocation unit) * (Bytes per sector) */
       /*mesg ("%12ld bytes in each allocation unit.",
                aulFSInfoBuf[1] * (USHORT)aulFSInfoBuf[4]);
       mesg ("%12ld total allocation units.", aulFSInfoBuf[2]);
       mesg ("%12ld available allocation units on disk.", aulFSInfoBuf[3]);
       mesg ("%12ld space free.", aulFSInfoBuf[3] * aulFSInfoBuf[1] * (USHORT)aulFSInfoBuf[4]);*/

    
void error(char *format, ...) {
  va_list args;
  char errstring[1024];

  va_start(args, format);
  vsprintf(errstring, format, args);
  va_end(args);

  success=1;
  installstate=ABORTED;
  WinMessageBox(HWND_DESKTOP, HWND_DESKTOP, errstring, INSTALLER_TITLE, 0, MB_OK | MB_ERROR | MB_MOVEABLE | MB_SYSTEMMODAL);
}

MRESULT	EXPENTRY ConfirmDlgProc(HWND hWnd, ULONG msg, MPARAM mp1,	MPARAM mp2)
{
SWP winpos;

  switch (msg) 
  {
  case WM_INITDLG:
     WinSetWindowText(hWnd, INSTALLER_TITLE);
     WinEnableWindow(WinWindowFromID(mainhwnd, I_Cancel), FALSE);
     WinSetDlgItemText(hWnd,	I_Confirm, confirmstring);
     WinQueryWindowPos(mainhwnd, &winpos);
     WinSetWindowPos(hWnd, HWND_TOP, winpos.x+30, winpos.y+30, 0, 0, SWP_MOVE | SWP_ZORDER);
     break;
  case WM_COMMAND:
     WinEnableWindow(WinWindowFromID(mainhwnd, I_Cancel), TRUE);
	switch ( SHORT1FROMMP(mp1) )
	    {
	    case I_Ja:
               WinDismissDlg(hWnd, 0);
               break;
	    case I_Alle:
               WinDismissDlg(hWnd, 1);
               break;
	    case I_Nein:
               WinDismissDlg(hWnd, 2);
               break;
	    case I_Halt:
               success=2;
               installstate=ABORTED;
               WinDismissDlg(hWnd, 3);
               break;
            }
     break;
   default :
       return(WinDefDlgProc(hWnd, msg, mp1, mp2));
   }
return(0L);
}

int confirm(char *format, ...) {
  va_list args;

  va_start(args, format);
  vsprintf(confirmstring, format, args);
  va_end(args);

  return WinDlgBox(HWND_DESKTOP, HWND_DESKTOP, &ConfirmDlgProc, NULLHANDLE, I_Karte, NULL);
}

void install_thread(void)
{
HAB ihab = 0;
HMQ ihmq = 0;
char *tmpdir, tmpfile[1024];
char tmpinstallpath[1024];
FILE *acefile;
int offset=0, k, j, installcount=0, installed=0;
ULONG   aulSysInfo[QSV_MAX] = {0};       /* System Information Data Buffer */


 ihab = WinInitialize(0);
 ihmq = WinCreateMsgQueue(ihab, 0);

 installstate = INSTALLING;

  DosQuerySysInfo(1L,                 /* Request all available system   */
                        QSV_MAX,            /* information                    */
                        (PVOID)aulSysInfo,
                        sizeof(ULONG)*QSV_MAX);

  instlog[0]=csfile[0]=bufile[0]=(char)('A'+(aulSysInfo[QSV_BOOT_DRIVE-1]-1));

 if((logfile=fopen(instlog, "ab"))==NULL)
    {
       error("Log file \"%s\" open failed! Installation aborted!", instlog);
       exit(1);
    }

 fprintf(logfile, "[%s]\r\n<Version>,%s\r\n<Start>\r\n", INSTALLER_APPLICATION, INSTALLER_VERSION);

 /* Create nested subdirectories if necessary. */
 strcpy(tmpinstallpath, installdir);
 for(k=3;k<strlen(installdir);k++)
    {
       if(tmpinstallpath[k] == '\\')
          {
             tmpinstallpath[k] = 0;
             if(DosCreateDir(tmpinstallpath, NULL)==NO_ERROR)
                fprintf(logfile, "<NewDir>,%s\r\n", tmpinstallpath);
             tmpinstallpath[k] = '\\';
          }
    }

 if(DosCreateDir(installdir, NULL)==NO_ERROR)
    fprintf(logfile, "<NewDir>,%s\r\n", installdir);

 if(strlen(installdir) > 0 && installdir[0] > 'a'-1 && installdir[0] < 'z'+1)
        installdir[0]=installdir[0] - ('a'-'A');
 if(strlen(installdir)>3 && installdir[1]==':' && installdir[2]=='\\')
    DosSetDefaultDisk((int)(installdir[0]-'A'+1));

 DosSetCurrentDir(installdir);

 /* Unpack files to destination directory */
 for(j=0;j<INSTALLER_PACKAGE_COUNT;j++)
 {
     if(packagesselected[j] == TRUE)
         installcount++;
 }
 if(installcount == 0)
 {
     mesg("No packages selected for installation!");
 }
 else
 {
     for(j=0;j<INSTALLER_PACKAGE_COUNT;j++)
     {
         if(packagesselected[j] == TRUE)
         {
             char statustext[512];
             aceseek_entry(j);
             resetglobals();
             sprintf(statustext,"Copying Files for %s %d/%d, Press Cancel to Abort.", INSTALLER_PACKAGES[j], installed+1, installcount);
             WinSetDlgItemText(mainhwnd,	I_Status, statustext);
             installer_unpack("Installer", 2); /* filename parameter has been deprecated but I am
                                                  leaving it there for the moment for simplification */
             installed++;
         }
     }
 }

 if(success==1)
    {
    WinMessageBox(HWND_DESKTOP, HWND_DESKTOP, "Error unpacking files, Installer may be corrupt!", INSTALLER_TITLE, 0, MB_OK | MB_ERROR | MB_MOVEABLE | MB_SYSTEMMODAL);
    }

 if(success==2)
    {
    WinMessageBox(HWND_DESKTOP, HWND_DESKTOP, "User aborted installation!", INSTALLER_TITLE, 0, MB_OK | MB_ERROR | MB_MOVEABLE | MB_SYSTEMMODAL);
    }

 if(installstate != ABORTED)
      installstate = COMPLETED;

 WinPostMsg(mainhwnd, WM_COMMAND, MPFROM2SHORT(I_Next, 0), MPFROM2SHORT(0, 0));

 WinDestroyMsgQueue(ihmq);
 WinTerminate(ihab);
}

void configsys_update(void)
{
  char *arg1, *arg2, *arg3;
  char temp[5000];
  int z, argn=0;

   readconfigsys();
  /* Update SET variables */
 if(strlen(INSTALLER_SETS)>0)
  {
  strcpy(temp, INSTALLER_SETS);
  arg1=&temp[0];
  arg2=arg3=NULL;
  for(z=0;z<strlen(INSTALLER_SETS);z++)
     {
       if(temp[z]==',')
          {
             argn++;
             temp[z]=0;
             switch(argn)
             {
             case 1:
                arg2=&temp[z+1];
                break;
             case 2:
                arg3=&temp[z+1];
                break;
             case 3:
                argn=0;
                updateset(arg1, arg2, (int)(arg3[0]-'0'));
                arg1=&temp[z+1];
                arg2=arg3=NULL;
                break;
             }
         }
     }
     updateset(arg1, arg2, (int)arg3-'0');
  }             
  /* Update system variables */
 if(strlen(INSTALLER_SYSVAR)>0)
  {
  strcpy(temp, INSTALLER_SYSVAR);
  arg1=&temp[0];
  arg2==NULL;
  for(z=0;z<strlen(INSTALLER_SYSVAR);z++)
     {
       if(temp[z]==',')
          {
             argn++;
             temp[z]=0;
             switch(argn)
             {
             case 1:
                arg2=&temp[z+1];
                break;
             case 2:
                argn=0;
                updatesys(arg1, arg2);
                arg1=&temp[z+1];
                arg2=NULL;
                break;
             }
         }
     }
     updatesys(arg1, arg2);
  }             

   writeconfigsys();
}

void create_wps_objects(void)
{
  char *arg1, *arg2, *arg3, *arg4, *arg5, *arg6;
  char temp[5000];
  char zerotext[2] = "";
  int z, argn;

  /* Create Folder Objects */
 if(strlen(INSTALLER_FOLDER)>0)
  {
  strcpy(temp, INSTALLER_FOLDER);
  argn=0;
  arg1=&temp[0];
  arg2=arg3=arg4=arg5=&zerotext[0];
  for(z=0;z<strlen(INSTALLER_FOLDER);z++)
     {
       if(temp[z]==',')
          {
             argn++;
             temp[z]=0;
             switch(argn)
             {
             case 1:
                arg2=&temp[z+1];
                break;
             case 2:
                arg3=&temp[z+1];
                break;
             case 3:
                arg4=&temp[z+1];
                break;
             case 4:
                arg5=&temp[z+1];
                break;
             case 5:
                argn=0;
                MakeFolder(arg1, arg2, arg3, arg4, arg5);
                arg1=&temp[z+1];
                arg2=arg3=arg4=arg5=&zerotext[0];
                break;
             }
         }
     }
     MakeFolder(arg1, arg2, arg3, arg4, arg5);
     fprintf(logfile, "<WPSFolderAdd>,%s,%s,%s,%s,%s\r\n", arg1, arg2,arg3,arg4,arg5);
  }

  /* Create Program Objects */
 if(strlen(INSTALLER_PROGRAM)>0)
  {
  strcpy(temp, INSTALLER_PROGRAM);
  argn=0;
  arg1=&temp[0];
  arg2=arg3=arg4=arg5=arg6=&zerotext[0];
  for(z=0;z<strlen(INSTALLER_PROGRAM);z++)
     {
       if(temp[z]==',')
          {
             argn++;
             temp[z]=0;
             switch(argn)
             {
             case 1:
                arg2=&temp[z+1];
                break;
             case 2:
                arg3=&temp[z+1];
                break;
             case 3:
                arg4=&temp[z+1];
                break;
             case 4:
                arg5=&temp[z+1];
                break;
             case 5:
                arg6=&temp[z+1];
                break;
             case 6:
                argn=0;
                MakeProgram(arg1, arg2, arg3, arg4, arg5, arg6);
                arg1=&temp[z+1];
                arg2=arg3=arg4=arg5=arg6=&zerotext[0];
                break;
             }
         }
     }
     MakeProgram(arg1, arg2, arg3, arg4, arg5, arg6);
     fprintf(logfile, "<WPSProgramAdd>,%s,%s,%s,%s,%s,%s\r\n", arg1, arg2,arg3,arg4,arg5,arg6);
  }             

  /* Create Shadow Objects */
 if(strlen(INSTALLER_SHADOW)>0)
  {
  strcpy(temp, INSTALLER_SHADOW);
  argn=0;
  arg1=&temp[0];
  arg2=arg3=&zerotext[0];
  for(z=0;z<strlen(INSTALLER_SHADOW);z++)
     {
       if(temp[z]==',')
          {
             argn++;
             temp[z]=0;
             switch(argn)
             {
             case 1:
                arg2=&temp[z+1];
                break;
             case 2:
                argn=0;
                MakeShadow(arg1, arg2, arg3);
                arg1=&temp[z+1];
                arg2=arg3=&zerotext[0];
                break;
             }
         }
     }
     MakeShadow(arg1, arg2, arg3);
     fprintf(logfile, "<WPSShadowAdd>,%s,%s,%s\r\n", arg1, arg2,arg3);
  }

}

void set_logo_pos(void)
{
SWP winpos;
int newx, newy;

        WinQueryWindowPos(mainhwnd, &winpos);
        newy=winpos.y+(winpos.cy-45)-INSTALLER_BITMAP_HEIGHT;
        if(winpos.cx < INSTALLER_BITMAP_WIDTH)
           newx=(int)(winpos.x-((float)(INSTALLER_BITMAP_WIDTH-winpos.cx)/2));
        else
           newx=(int)(winpos.x+((winpos.cx-INSTALLER_BITMAP_WIDTH)/2));
        WinSetWindowPos(logohwnd, HWND_TOP, newx, newy, INSTALLER_BITMAP_WIDTH, INSTALLER_BITMAP_HEIGHT, SWP_SIZE | SWP_MOVE | SWP_ZORDER);
        WinSetWindowPos(mainhwnd, logohwnd, 0, 0, 0, 0, SWP_ZORDER);
}

MRESULT	EXPENTRY PackageDlgProc(HWND hWnd, ULONG msg, MPARAM mp1,	MPARAM mp2)
{
    int j, i, count = 0;
    long spacefree;
    char buffer[256];
    static int drivelist[26];

switch ( msg )
   {
   case	WM_INITDLG :
       for(j=0;j<INSTALLER_PACKAGE_COUNT;j++)
           WinSendMsg(WinWindowFromID(hWnd, PACKAGES), LM_INSERTITEM, MPFROMSHORT(LIT_END), MPFROMP(INSTALLER_PACKAGES[j]));
	break;
			/* Process push	button selections		*/
   case	WM_CLOSE :
       for(j=0;j<INSTALLER_PACKAGE_COUNT;j++)
           packagesselected[j] = FALSE;
       i = (int)WinSendMsg(WinWindowFromID(hWnd, PACKAGES),
                           LM_QUERYSELECTION,
                           MPFROMSHORT(LIT_FIRST),0L);
       while(i!=LIT_NONE)
       {
           packagesselected[i] = TRUE;
           i = (int)WinSendMsg(WinWindowFromID(hWnd, PACKAGES),
                               LM_QUERYSELECTION,
                                        (MPARAM)i,0L);
       }
       for(j=0;j<INSTALLER_PACKAGE_COUNT;j++)
       {
           if(packagesselected[j] == TRUE)
               count++;
       }
       if(count == 0)
           packagechosen=FALSE;
       else
           packagechosen=TRUE;

       WinDismissDlg(hWnd, FALSE);
	break;
   case	WM_COMMAND :
	switch ( SHORT1FROMMP(mp1) )
	    {
            case PB_OK:
                for(j=0;j<INSTALLER_PACKAGE_COUNT;j++)
                    packagesselected[j] = FALSE;
                i = (int)WinSendMsg(WinWindowFromID(hWnd, PACKAGES),
                                    LM_QUERYSELECTION,
                                    MPFROMSHORT(LIT_FIRST),0L);
                while(i!=LIT_NONE)
                {
                    packagesselected[i] = TRUE;
                    i = (int)WinSendMsg(WinWindowFromID(hWnd, PACKAGES),
                                    LM_QUERYSELECTION,
                                        (MPARAM)i,0L);
                }
                for(j=0;j<INSTALLER_PACKAGE_COUNT;j++)
                {
                    if(packagesselected[j] == TRUE)
                        count++;
                }
                if(count == 0)
                    packagechosen=FALSE;
                else
                    packagechosen=TRUE;
                
                WinDismissDlg(hWnd, FALSE);
                break;
            case PB_CANCEL:
                WinDismissDlg(hWnd, FALSE);
                break;
            case PB_SELECTALL:
                for(j=0;j<INSTALLER_PACKAGE_COUNT;j++)
                    WinSendMsg(WinWindowFromID(hWnd, PACKAGES), LM_SELECTITEM, MPFROMSHORT(j), (MPARAM)TRUE);
                break;
            case PB_DESELECTALL:
                WinSendMsg(WinWindowFromID(hWnd, PACKAGES), LM_SELECTITEM, MPFROMSHORT(LIT_NONE), FALSE);
                break;
            }
        break;
			/* Pass	through	unhandled messages		*/
   default :
       return(WinDefDlgProc(hWnd, msg, mp1, mp2));
   }
return(0L);
}

MRESULT	EXPENTRY DriveDlgProc(HWND hWnd, ULONG msg, MPARAM mp1,	MPARAM mp2)
{
    int j, i, count = 0;
    unsigned long long spacefree;
    char buffer[256];
    static int drivelist[26];

switch ( msg )
   {
   case	WM_INITDLG :
       for(j=3;j<27;j++)
       {
           spacefree = drivefree(j);
           if(spacefree > 0)
           {
               drivelist[count] = j;
               count++;
               sprintf(buffer, "Drive %c: %llu bytes free.", ('A'+j)-1, spacefree);
               WinSendMsg(WinWindowFromID(hWnd, DRIVELIST), LM_INSERTITEM, MPFROMSHORT(LIT_END), MPFROMP(buffer));
           }
       }
	break;
			/* Process push	button selections		*/
   case	WM_CLOSE :
       i = (int)WinSendMsg(WinWindowFromID(hWnd, DRIVELIST),
							  LM_QUERYSELECTION,
							  MPFROMSHORT(LIT_CURSOR),0L);
       	WinDismissDlg(hWnd, drivelist[i]);
	break;
   case	WM_COMMAND :
	switch ( SHORT1FROMMP(mp1) )
	    {
            case PB_OK:
                i = (int)WinSendMsg(WinWindowFromID(hWnd, DRIVELIST),
                                    LM_QUERYSELECTION,
                                    MPFROMSHORT(LIT_CURSOR),0L);
                if(i>-1)
                    installdir[0] = ('A'+drivelist[i])-1;
                driveselected = TRUE;
                WinDismissDlg(hWnd, FALSE);
                break;
            case PB_CANCEL:
                driveselected = FALSE;
                WinDismissDlg(hWnd, FALSE);
                break;
            }
        break;
			/* Pass	through	unhandled messages		*/
   default :
       return(WinDefDlgProc(hWnd, msg, mp1, mp2));
   }
return(0L);
}

MRESULT	EXPENTRY LogoDlgProc(HWND hWnd, ULONG msg, MPARAM mp1,	MPARAM mp2)
{

switch ( msg )
   {
   case	WM_INITDLG :
        logohwnd = hWnd;
        set_logo_pos();
	break;
			/* Process push	button selections		*/
   case	WM_CLOSE :
	WinDismissDlg(hWnd, FALSE);
	break;

			/* Pass	through	unhandled messages		*/
   default :
       return(WinDefDlgProc(hWnd, msg, mp1, mp2));
   }
return(0L);
}

void logo_thread(void)
{
 HAB lhab;
 HMQ lhmq;
 
 /* Initialization */
 lhab = WinInitialize(0);
 lhmq = WinCreateMsgQueue(hab, 0);

 /* Create main dialog window */
 WinDlgBox(HWND_DESKTOP, HWND_DESKTOP, &LogoDlgProc, NULLHANDLE, I_BMPWin, NULL);

 /* Cleanup */
 WinDestroyMsgQueue(lhmq);
 WinTerminate(lhab);
}

MRESULT	EXPENTRY InstallerDlgProc(HWND hWnd, ULONG msg, MPARAM mp1,	MPARAM mp2)
{
CHAR szBuffer[512];		   /* String Buffer			*/
HPOINTER icon;
ULONG sliderpos;

switch ( msg )
   {
			/* Perform dialog initialization		*/
   case	WM_INITDLG :
        mainhwnd = hWnd;
        WinSetWindowText(hWnd, INSTALLER_TITLE);
        icon = WinLoadPointer(HWND_DESKTOP,NULLHANDLE,I_Icon);
        WinSendMsg(hWnd, WM_SETICON, (MPARAM)icon, 0);
        WinEnableWindow(WinWindowFromID(hWnd, I_Back), FALSE);
        _beginthread((void *)logo_thread, NULL, (4*0xFFFF), NULL);
        drivefree(3);
	break;
			/* Process control selections			*/
   case	WM_CONTROL :
	switch ( SHORT2FROMMP(mp1) )
	    {
	    }
	break;
   case WM_WINDOWPOSCHANGED:
        set_logo_pos();
        return WinDefDlgProc(hWnd, msg, mp1, mp2);
        break;
			/* Process push	button selections		*/
   case	WM_COMMAND :
	switch ( SHORT1FROMMP(mp1) )
	    {
	    case I_Cancel:
               if(installstate == INSTALLING)
                  installstate = ABORTED;
               else
                  {
                  WinSendMsg(logohwnd, WM_CLOSE, 0, 0);
	          WinDismissDlg(hWnd, FALSE);
                  }
	       break;
            case I_Back:
               installstage=installstage-2;
               WinPostMsg(hWnd, WM_COMMAND, MPFROM2SHORT(I_Next, 0), MPFROM2SHORT(0, 0));
               break;
            case I_Next:
               installstage++;
               switch (installstage) 
               {
               case 0:                
                  WinEnableWindow(WinWindowFromID(hWnd, I_Back), FALSE);
                  WinEnableWindow(WinWindowFromID(hWnd, I_Next), TRUE);
                  WinSetDlgItemText(hWnd,	I_Status, "Press Next to begin the installation...");
                  break;
               case 1:
                  WinEnableWindow(WinWindowFromID(hWnd, I_Back), FALSE);
                  WinEnableWindow(WinWindowFromID(hWnd, I_Next), FALSE);
                  WinEnableWindow(WinWindowFromID(hWnd, I_Cancel), FALSE);
                  WinSetDlgItemText(hWnd,	I_Status, "Select Packages for Installation...");
                  if(WinDlgBox(HWND_DESKTOP, HWND_DESKTOP, &PackageDlgProc, NULLHANDLE, DLG_PACKAGESTOINSTALL, NULL) == DID_ERROR || packagechosen == FALSE)
                  {
                      if(packagechosen == FALSE)
                      {
                          mesg("No packages selected for installation!");
                      }
                      installstage--;
                      WinEnableWindow(WinWindowFromID(hWnd, I_Back), FALSE);
                      WinEnableWindow(WinWindowFromID(hWnd, I_Next), TRUE);
                      WinEnableWindow(WinWindowFromID(hWnd, I_Cancel), TRUE);
                      WinSetDlgItemText(hWnd,	I_Status, "Press Next to begin the installation...");
                  }
                  else
                  {
                      WinSetDlgItemText(hWnd,	I_Status, "Confirm Destination Location...");
                      if(WinDlgBox(HWND_DESKTOP, HWND_DESKTOP, &DriveDlgProc, NULLHANDLE, DLG_SELECTINSTALLATIONDRIVE, NULL) == DID_ERROR || driveselected == FALSE)
                      {
                          installstage--;
                          WinEnableWindow(WinWindowFromID(hWnd, I_Back), FALSE);
                          WinEnableWindow(WinWindowFromID(hWnd, I_Next), TRUE);
                          WinEnableWindow(WinWindowFromID(hWnd, I_Cancel), TRUE);
                          WinSetDlgItemText(hWnd,	I_Status, "Press Next to begin the installation...");
                      }
                      else
                      {
                          if(WinDlgBox(HWND_DESKTOP, HWND_DESKTOP, &DestDlgProc, NULLHANDLE, I_Dest, NULL)==DID_ERROR)
                          {
                              DosBeep(100,200);
                          }
                          WinEnableWindow(WinWindowFromID(hWnd, I_Back), TRUE);
                          WinEnableWindow(WinWindowFromID(hWnd, I_Next), TRUE);
                          WinEnableWindow(WinWindowFromID(hWnd, I_Cancel), TRUE);
                          WinSetDlgItemText(hWnd,	I_Status, "To Begin Copying Files Press Next...");
                      }
                  }
                  break;
               case 2:
                  WinEnableWindow(WinWindowFromID(hWnd, I_Back), FALSE);
                  WinEnableWindow(WinWindowFromID(hWnd, I_Next), FALSE);
                  WinSetDlgItemText(hWnd,	I_Status, "Copying Files, Press Cancel to Abort.");

   	          _beginthread((void *)install_thread, NULL, (4*0xFFFF), NULL);

                  break;
               case 3:
                  WinEnableWindow(WinWindowFromID(hWnd, I_Back), FALSE);
                  WinEnableWindow(WinWindowFromID(hWnd, I_Next), FALSE);
                  WinSetDlgItemText(hWnd,	I_Status, "Files finished Copying.");
                  WinSetDlgItemText(hWnd,	I_Next, "Next >");
                  if(installstate == COMPLETED)
                  {
                      if(strlen(INSTALLER_PROGRAM) > 0 || strlen(INSTALLER_FOLDER) > 0 || strlen(INSTALLER_SHADOW) >0)
                          if(WinMessageBox(HWND_DESKTOP, HWND_DESKTOP, "Do you wish to make WPS items on your desktop?", INSTALLER_TITLE, 0, MB_YESNO | MB_INFORMATION | MB_MOVEABLE | MB_SYSTEMMODAL)==MBID_YES)
                              create_wps_objects();
                      if(strlen(INSTALLER_SETS) > 0 || strlen(INSTALLER_SYSVAR) > 0)
                          if(WinMessageBox(HWND_DESKTOP, HWND_DESKTOP, "Do you wish to add the required entries to your CONFIG.SYS?", INSTALLER_TITLE, 0, MB_YESNO | MB_INFORMATION | MB_MOVEABLE | MB_SYSTEMMODAL)==MBID_YES)
                              configsys_update();
                     }
                  else
                     {
                     if(WinMessageBox(HWND_DESKTOP, HWND_DESKTOP, "Do you wish to delete the files that were already copied?", INSTALLER_TITLE, 0, MB_YESNO | MB_INFORMATION | MB_MOVEABLE | MB_SYSTEMMODAL)==MBID_YES)
                        {
                        fprintf(logfile, "<Removed>\r\n<End>\r\n");
                        fclose(logfile);
                        delete_files();
                        current_file=0;
                        WinPostMsg(mainhwnd, WM_USER, 0, 0);
                        }
                     }
                  WinPostMsg(hWnd, WM_COMMAND, MPFROM2SHORT(I_Next, 0), MPFROM2SHORT(0, 0));
                  break;
               case 4:
                  WinEnableWindow(WinWindowFromID(hWnd, I_Back), FALSE);
                  WinEnableWindow(WinWindowFromID(hWnd, I_Next), TRUE);
                  WinSetDlgItemText(hWnd,	I_Status, "Installation has completed, press Finish to close Install.");
                  WinSetDlgItemText(hWnd,	I_Next, "Finish");
                  if(files_deleted==0)
                     {
                     fprintf(logfile, "<End>\r\n");
                     fclose(logfile);
                     }
                  break;
               case 5:
                  WinSendMsg(logohwnd, WM_CLOSE, 0, 0);
	          WinDismissDlg(hWnd, FALSE);
                  break;
               }
               break;
	    }
	break;
   case WM_USER:
        if(pixels==0)
           pixels = SHORT2FROMMP(WinSendMsg(WinWindowFromID(hWnd, I_Percent), SLM_QUERYSLIDERINFO, MPFROM2SHORT(SMA_SLIDERARMPOSITION,SMA_RANGEVALUE), 0));
        if(files!=0)
            sliderpos = (int)(((float)(current_file)/(float)files)*pixels);
        /*mesg("sliderpos %d current_file %d files %d pixels %d", sliderpos, current_file, files, pixels);*/
        WinSendMsg(WinWindowFromID(hWnd, I_Percent), SLM_SETSLIDERINFO, MPFROM2SHORT(SMA_SLIDERARMPOSITION,SMA_RANGEVALUE), (MPARAM)sliderpos-1);
        break;
			/* Close requested, exit dialogue		*/
   case	WM_CLOSE :
	WinDismissDlg(hWnd, FALSE);
	break;

			/* Pass	through	unhandled messages		*/
   default :
       return(WinDefDlgProc(hWnd, msg, mp1, mp2));
   }
return(0L);
}

int MakeFolder(char Title[], char Icon[], char dest[], char id[], char setup[])
{
  char szArg[200];

  memset(szArg,0,sizeof(szArg));

  if ((Icon != NULL) && (strlen(Icon) != 0))
   { strcat(szArg,"ICONFILE=");
     strcat(szArg,installdir);
     strcat(szArg,"\\");
     strcat(szArg,Icon);
   }

  if ((id != NULL) && (strlen(id) != 0))
   { strcat(szArg,";OBJECTID=");
     strcat(szArg,id);
   }

  if ((setup != NULL) && (strlen(setup) != 0))
   { strcat(szArg,";");
     strcat(szArg,setup);
   }

  WinCreateObject("WPFolder",Title,szArg,dest,CO_REPLACEIFEXISTS);
}

int MakeProgram(char Title[], char Program[], char Icon[], char dest[], char id[], char setup[])
{
  char szArg[200];

  memset(szArg,0,sizeof(szArg));

  strcat(szArg,"EXENAME=");
  strcat(szArg,installdir);
  strcat(szArg,"\\");
  strcat(szArg,Program);

  if ((Icon != NULL) && (strlen(Icon) != 0))
   { strcat(szArg,";ICONFILE=");
     strcat(szArg,installdir);
     strcat(szArg,"\\");
     strcat(szArg,Icon);
   }

  if ((id != NULL) && (strlen(id) != 0))
   { strcat(szArg,";OBJECTID=");
     strcat(szArg,id);
   }

  if ((setup != NULL) && (strlen(setup) != 0))
   { strcat(szArg,";");
     strcat(szArg,setup);
   }

  WinCreateObject("WPProgram",Title,szArg,dest,CO_REPLACEIFEXISTS);
}

int MakeShadow(char Title[], char reference[], char dest[])
{
  char szArg[200];

  memset(szArg,0,sizeof(szArg));

  strcat(szArg,"SHADOWID=");
  strcat(szArg,installdir);
  strcat(szArg,"\\");
  strcat(szArg,reference);
  WinCreateObject("WPShadow",Title,szArg,dest,CO_REPLACEIFEXISTS);
}


int main(int argc, char *argv[])
{
 /* Initialization */
 hab = WinInitialize(0);
 hmq = WinCreateMsgQueue(hab, 0);
 if((self = fopen(argv[0], "rb")) == NULL)
 {
     mesg("Could not open SFX archive for reading!");
     exit(1);
 }
 if(loadheader() == FALSE)
 {
     mesg("Could not load all required variables!");
     exit(3);
 }
 strcpy(installdir, INSTALLER_PATH);
 /* Create main dialog window */
 if(WinDlgBox(HWND_DESKTOP, HWND_DESKTOP, &InstallerDlgProc, NULLHANDLE, I_Dialog, NULL)==DID_ERROR)
    {
       DosBeep(100,200);
    }

 /* Cleanup */
 WinDestroyMsgQueue(hmq);
 WinTerminate(hab);
 return 0;
}
