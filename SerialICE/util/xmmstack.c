/*
    XMM-STACK: convert the stack to a XMM registers for gcc x86 assembler codes
    Copyright (C) 2008  Urbez Santana i Roma

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdlib.h>
#include <unistd.h>
// #include <pty.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <pcre.h>


int mode_xmm=0;

#ifdef __APPLE__
size_t strnlen(const char *string, size_t maxlen)
{
	const char *end = memchr (string, '\0', maxlen);
	return end ? (size_t) (end - string) : maxlen;
}

char *strndup (const char *s, size_t n)
{
	size_t len = strnlen (s, n);
	char *duplicate = malloc (len + 1);
	if (duplicate == NULL)
		return NULL;

	duplicate[len] = '\0';
	return memcpy (duplicate, s, len);
}
#endif

int regexp(const char *pattern,char *search,int len,char **found)
{
    pcre *r;
    const char *err;
    int errpos;
    int ret;
    int fnd[64];
    int i;
    for (i=0;found[i];i++) free(found[i]);found[0]=0;
    r=pcre_compile(pattern,PCRE_MULTILINE,&err,&errpos,NULL);//Default char table, (utf8 millor?)
    if (!r) {fprintf(stderr,"%s,%d\n",err,errpos);return 0;}
    ret=pcre_exec(r,NULL,search,len,0,0, fnd, 64);
    if (ret<=0) return 0;
    for (i=0;i<ret;i++) found[i]=strndup(search+fnd[i*2],fnd[i*2+1]-fnd[i*2]);
    found[ret]=0;
    return fnd[1];//el que ha llegit
}

char buf[4097];
int  num;

void chomp(char *str)
{
    char *rd,*wr;
    rd=wr=str;
    while(*rd && *rd>=0 && *rd<=32) rd++;
    while(*rd) *wr++=*rd++;
    while(wr>str && wr[-1]>=0 && wr[-1]<=32) wr--;
    *wr=0;
}

int instrlen(const char *inst,int wr)
{
    int len=strlen(inst);
    if (!wr && !strcmp(inst,"movzbl")) return 1;
    if (!wr && !strcmp(inst,"movzwl")) return 2;
    if (!wr && !strcmp(inst,"movsbl")) return 1;
    if (!wr && !strcmp(inst,"movswl")) return 2;
    if (len<=0) return 1;
    if (inst[len-1]=='l') return 4;
    if (inst[len-1]=='w') return 2;
    return 1;
}

#define MSG_INSTR "^\\([^\\[]*\\[0x([0-9a-f]*)\\] .*\\): (\\w+)\\s+(;|([^,]+);|(.*),(.*);).*$"
#define MSG_PARAM "^0x([0-9a-f]+)\\s+\\d+\\s*$"


char *find[64];
int len;

int main(int argc,char **argv)
{
    int i;
    FILE *f,*w;
    char *FileName;
    int param;
    int inapp;

    if (argc<2)
    {
      printf("Usage: \n\txmmstack [-xmm] [-mmx] file.s\nThis program is for use with:\n"
             "\tgcc -fomit-frame-pointer -fno-stack-protector *.c\n"
             "\tyou can use -O3 and any other gcc option or g++ only\n"
             "\tif not changes the use of the stack\n");
      return 0;
    }
    FileName=argv[1];
    mode_xmm=0;
    inapp=0;
    for (param=1;param<argc;param++)
    {
        if (!strcasecmp(argv[param],"-xmm"))
        {
            mode_xmm=1;
            continue;
        }
        if (!strcasecmp(argv[param],"-mmx"))
        {
            mode_xmm=0;
            continue;
        }
        if (*argv[param]=='-')
        {
            printf("Incorrect option: %s\n",argv[param]);
            return 0;
        }
        FileName=argv[param];
    }


    f=fopen(FileName,"rb");
    if (!f) {printf("File %s, not found\n",FileName);return 0;}
    sprintf(buf,"%sn.s",FileName);
    w=fopen(buf,"wb+");
    if (!w) {printf("Cannot create %s\n",buf);return 0;}

    fprintf(w,".text\n"
	          ".p2align 4,,15\n\n\n");

//DR0, save eax
//DR1, save edx
//DR2, stack pointer of xmm in byte dr2[0] , final stack offset of xmm in byte dr2[1]
//DR3, save esp
//EAX, calc and, set register
//EDX, offset for jumps
//ESP, return address
//use of leal for multiply and add, for not affect the flags.
//movzbl also not affect flags
//bswap and xchg also not affect flags.
//dr2 must be initially 0x7f

    fprintf(w,"docall:\nmovl %%eax,%%dr0 #NOSTACK\n\
    movl %%dr2,%%eax #NOSTACK\n\
    movzbl %%al,%%eax #NOSTACK\n\
    leal -4(%%eax),%%eax #NOSTACK\n\
    movb %%al,%%ah #NOSTACK\n\
    movl %%eax,%%dr2 #NOSTACK\n\
    leal 8(%%esp),%%eax #NOSTACK\n\
    jmp setxmml\n\
    dopop:\n movl %%eax,%%dr0 #NOSTACK\n\
    movl %%dr2,%%eax #NOSTACK\n\
    movzbl %%al,%%eax #NOSTACK\n\
    movb %%al,%%ah #NOSTACK\n\
    leal 4(%%eax),%%eax #NOSTACK\n\
    movl %%eax,%%dr2 #NOSTACK\n\
    jmp getxmml\n\
    dopush:\n movl %%eax,%%dr3 #NOSTACK\n\
    movl %%dr2,%%eax #NOSTACK\n\
    movzbl %%al,%%eax #NOSTACK\n\
    leal -4(%%eax),%%eax #NOSTACK\n\
    movb %%al,%%ah #NOSTACK\n\
    movl %%eax,%%dr2 #NOSTACK\n\
    movl %%dr3,%%eax\n\
    jmp setxmml\n");

    //16 bytes table xmm/12 mmx, pextrw and pinsrw consumes 5 bytes, and jmp *esp consumes 2, nop consumes 1 byte, movb consumes 2 bytes
    //Align of each,must 16 bytes, for use 1,2,4,8 escalar multiple.
    fprintf(w,"setxmm:\n");
    if (mode_xmm)
        fprintf(w,"setxmmover:\n"
                  "jmp setxmmover\n"
                  ".long 0x90909090\n"
                  ".long 0x90909090\n"
                  ".long 0x90909090\n"
                  ".word 0x9090\n");
    else
        fprintf(w,"setxmmover:\n"
                  "jmp setxmmover\n"
                  ".long 0x90909090\n"
                  ".long 0x90909090\n"
                  ".word 0x9090\n");
    for (i=1;i<127;i++)
    {
        if (mode_xmm)
        {
            fprintf(w,"pextrw $%d,%%xmm%d,%%edx\n"
                      "%s\n"
                      "pinsrw $%d,%%edx,%%xmm%d\n"
                      "jmp *%%esp\n"
                      "nop\n"
                      "nop\n"
                     ,(i/2)&7,i/16,(i&1)?"movb %al,%dh":"movb %al,%dl",(i/2)&7,i/16);
        }
        else
        {
            fprintf(w,"pextrw $%d,%%mm%d,%%edx\n"
                      "%s\n"
                      "pinsrw $%d,%%edx,%%mm%d\n"
                      "jmp *%%esp\n"
                     ,(i/2)&3,(i/8)&7,(i&1)?"movb %al,%dh":"movb %al,%dl",(i/2)&3,(i/8)&7);
        }
    }
    if (mode_xmm)
        fprintf(w,"setxmmunder:\n"
                  "jmp setxmmunder\n"
                  ".long 0x90909090\n"
                  ".long 0x90909090\n"
                  ".long 0x90909090\n"
                  ".word 0x9090\n");
    else
        fprintf(w,"setxmmunder:\n"
                  "jmp setxmmunder\n"
                  ".long 0x90909090\n"
                  ".long 0x90909090\n"
                  ".word 0x9090\n");
    //10 bytes table xmm/8 mmx, pextrw and pinsrw consumes 5 bytes, and jmp *esp consumes 2, nop consumes 1 byte, movb consumes 2 bytes
    fprintf(w,"getxmm:\n");
    if (mode_xmm)
        fprintf(w,"getxmmover:\n"
                  "jmp getxmmover\n"
                  ".long 0x90909090\n"
                  ".long 0x90909090\n");
    else
        fprintf(w,"getxmmover:\n"
                  "jmp getxmmover\n"
                  ".long 0x90909090\n"
                  ".word 0x9090\n");
    for (i=1;i<127;i++)
    {
        if (mode_xmm)
        {
            fprintf(w,"pextrw $%d,%%xmm%d,%%edx\n"
                      "%s\n"
                      "jmp *%%esp\n"
                      "nop\n"
                     ,(i/2)&7,i/16,(i&1)?"movb %dh,%al":"movb %dl,%al");
        }
        else
        {
            fprintf(w,"pextrw $%d,%%mm%d,%%edx\n"
                      "%s\n"
                      "jmp *%%esp\n"
                     ,(i/2)&3,(i/8)&7,(i&1)?"movb %dh,%al":"movb %dl,%al");
        }
    }
    if (mode_xmm)
        fprintf(w,"getxmmunder:\n"
                  "jmp getxmmunder\n"
                  ".long 0x90909090\n"
                  ".long 0x90909090\n");
    else
        fprintf(w,"getxmmunder:\n"
                  "jmp getxmmunder\n"
                  ".long 0x90909090\n"
                  ".word 0x9090\n");
//setxmmb: al=byte to set, byte dr2[1]=offset of xmm stack
    if (mode_xmm)
        fprintf(w,
"\
setxmmb:\n\
    movl %%edx,%%dr1\n\
    movl %%esp,%%dr3\n\
    movl %%dr2,%%edx\n\
    movzbl %%dh,%%edx\n\
    leal (,%%edx,4),%%edx\n\
    leal setxmm(,%%edx,4),%%edx\n\
    movl $.+7,%%esp\n\
    jmp *%%edx\n\
    movl %%dr1,%%edx\n\
    movl %%dr3,%%esp\n\
    jmp *%%esp\n\
getxmmb:\n\
    movl %%edx,%%dr1\n\
    movl %%esp,%%dr3\n\
    movl %%dr2,%%edx\n\
    movzbl %%dh,%%edx\n\
    leal (%%edx,%%edx,4),%%edx\n\
    leal getxmm(,%%edx,2),%%edx\n\
    movl $.+7,%%esp\n\
    jmp *%%edx\n\
    movl %%dr1,%%edx\n\
    movl %%dr3,%%esp\n\
    jmp *%%esp\n\
setxmmw:\n\
    movl %%edx,%%dr1\n\
    movl %%esp,%%dr3\n\
    movl %%dr2,%%edx\n\
    movzbl %%dh,%%edx\n\
    leal (,%%edx,4),%%edx\n\
    leal setxmm(,%%edx,4),%%edx\n\
    movl $.+7,%%esp\n\
    jmp *%%edx\n\
    movb %%ah,%%al\n\
    movl %%dr2,%%edx\n\
    movzbl %%dh,%%edx\n\
    leal 1(%%edx),%%edx\n\
    leal (,%%edx,4),%%edx\n\
    leal setxmm(,%%edx,4),%%edx\n\
    movl $.+7,%%esp\n\
    jmp *%%edx\n\
    movl %%dr1,%%edx\n\
    movl %%dr3,%%esp\n\
    jmp *%%esp\n\
getxmmw:\n\
    movl %%edx,%%dr1\n\
    movl %%esp,%%dr3\n\
    movl %%dr2,%%edx\n\
    movzbl %%dh,%%edx\n\
    leal 1(%%edx),%%edx\n\
    leal (%%edx,%%edx,4),%%edx\n\
    leal getxmm(,%%edx,2),%%edx\n\
    movl $.+7,%%esp\n\
    jmp *%%edx\n\
    movb %%al,%%ah\n\
    movl %%dr2,%%edx\n\
    movzbl %%dh,%%edx\n\
    leal (%%edx,%%edx,4),%%edx\n\
    leal getxmm(,%%edx,2),%%edx\n\
    movl $.+7,%%esp\n\
    jmp *%%edx\n\
    movl %%dr1,%%edx\n\
    movl %%dr3,%%esp\n\
    jmp *%%esp\n\
setxmml:\n\
    movl %%edx,%%dr1\n\
    movl %%esp,%%dr3\n\
    movl %%dr2,%%edx\n\
    movzbl %%dh,%%edx\n\
    leal (,%%edx,4),%%edx\n\
    leal setxmm(,%%edx,4),%%edx\n\
    movl $.+7,%%esp\n\
    jmp *%%edx\n\
    movb %%ah,%%al\n\
    movl %%dr2,%%edx\n\
    movzbl %%dh,%%edx\n\
    leal 1(%%edx),%%edx\n\
    leal (,%%edx,4),%%edx\n\
    leal setxmm(,%%edx,4),%%edx\n\
    movl $.+7,%%esp\n\
    jmp *%%edx\n\
    bswapl %%eax\n\
    movl %%dr2,%%edx\n\
    movzbl %%dh,%%edx\n\
    leal 3(%%edx),%%edx\n\
    leal (,%%edx,4),%%edx\n\
    leal setxmm(,%%edx,4),%%edx\n\
    movl $.+7,%%esp\n\
    jmp *%%edx\n\
    movb %%ah,%%al\n\
    movl %%dr2,%%edx\n\
    movzbl %%dh,%%edx\n\
    leal 2(%%edx),%%edx\n\
    leal (,%%edx,4),%%edx\n\
    leal setxmm(,%%edx,4),%%edx\n\
    movl $.+7,%%esp\n\
    jmp *%%edx\n\
    movl %%dr1,%%edx\n\
    movl %%dr3,%%esp\n\
    jmp *%%esp\n\
getxmml:\n\
    movl %%edx,%%dr1\n\
    movl %%esp,%%dr3\n\
    movl %%dr2,%%edx\n\
    movzbl %%dh,%%edx\n\
    leal 2(%%edx),%%edx\n\
    leal (%%edx,%%edx,4),%%edx\n\
    leal getxmm(,%%edx,2),%%edx\n\
    movl $.+7,%%esp\n\
    jmp *%%edx\n\
    movb %%al,%%ah\n\
    movl %%dr2,%%edx\n\
    movzbl %%dh,%%edx\n\
    leal 3(%%edx),%%edx\n\
    leal (%%edx,%%edx,4),%%edx\n\
    leal getxmm(,%%edx,2),%%edx\n\
    movl $.+7,%%esp\n\
    jmp *%%edx\n\
    bswapl %%eax\n\
    movl %%dr2,%%edx\n\
    movzbl %%dh,%%edx\n\
    leal 1(%%edx),%%edx\n\
    leal (%%edx,%%edx,4),%%edx\n\
    leal getxmm(,%%edx,2),%%edx\n\
    movl $.+7,%%esp\n\
    jmp *%%edx\n\
    movb %%al,%%ah\n\
    movl %%dr2,%%edx\n\
    movzbl %%dh,%%edx\n\
    leal (%%edx,%%edx,4),%%edx\n\
    leal getxmm(,%%edx,2),%%edx\n\
    movl $.+7,%%esp\n\
    jmp *%%edx\n\
    movl %%dr1,%%edx\n\
    movl %%dr3,%%esp\n\
    jmp *%%esp\n"
        );
    else
        fprintf(w,
"\
setxmmb:\n\
    movl %%edx,%%dr1\n\
    movl %%esp,%%dr3\n\
    movl %%dr2,%%edx\n\
    movzbl %%dh,%%edx\n\
    leal (%%edx,%%edx,2),%%edx\n\
    leal setxmm(,%%edx,4),%%edx\n\
    movl $.+7,%%esp\n\
    jmp *%%edx\n\
    movl %%dr1,%%edx\n\
    movl %%dr3,%%esp\n\
    jmp *%%esp\n\
getxmmb:\n\
    movl %%edx,%%dr1\n\
    movl %%esp,%%dr3\n\
    movl %%dr2,%%edx\n\
    movzbl %%dh,%%edx\n\
    leal getxmm(,%%edx,8),%%edx\n\
    movl $.+7,%%esp\n\
    jmp *%%edx\n\
    movl %%dr1,%%edx\n\
    movl %%dr3,%%esp\n\
    jmp *%%esp\n\
setxmmw:\n\
    movl %%edx,%%dr1\n\
    movl %%esp,%%dr3\n\
    movl %%dr2,%%edx\n\
    movzbl %%dh,%%edx\n\
    leal (%%edx,%%edx,2),%%edx\n\
    leal setxmm(,%%edx,4),%%edx\n\
    movl $.+7,%%esp\n\
    jmp *%%edx\n\
    movb %%ah,%%al\n\
    movl %%dr2,%%edx\n\
    movzbl %%dh,%%edx\n\
    leal 1(%%edx),%%edx\n\
    leal (%%edx,%%edx,2),%%edx\n\
    leal setxmm(,%%edx,4),%%edx\n\
    movl $.+7,%%esp\n\
    jmp *%%edx\n\
    movl %%dr1,%%edx\n\
    movl %%dr3,%%esp\n\
    jmp *%%esp\n\
getxmmw:\n\
    movl %%edx,%%dr1\n\
    movl %%esp,%%dr3\n\
    movl %%dr2,%%edx\n\
    movzbl %%dh,%%edx\n\
    leal 1(%%edx),%%edx\n\
    leal getxmm(,%%edx,8),%%edx\n\
    movl $.+7,%%esp\n\
    jmp *%%edx\n\
    movb %%al,%%ah\n\
    movl %%dr2,%%edx\n\
    movzbl %%dh,%%edx\n\
    leal getxmm(,%%edx,8),%%edx\n\
    movl $.+7,%%esp\n\
    jmp *%%edx\n\
    movl %%dr1,%%edx\n\
    movl %%dr3,%%esp\n\
    jmp *%%esp\n\
setxmml:\n\
    movl %%edx,%%dr1\n\
    movl %%esp,%%dr3\n\
    movl %%dr2,%%edx\n\
    movzbl %%dh,%%edx\n\
    leal (%%edx,%%edx,2),%%edx\n\
    leal setxmm(,%%edx,4),%%edx\n\
    movl $.+7,%%esp\n\
    jmp *%%edx\n\
    movb %%ah,%%al\n\
    movl %%dr2,%%edx\n\
    movzbl %%dh,%%edx\n\
    leal 1(%%edx),%%edx\n\
    leal (%%edx,%%edx,2),%%edx\n\
    leal setxmm(,%%edx,4),%%edx\n\
    movl $.+7,%%esp\n\
    jmp *%%edx\n\
    bswapl %%eax\n\
    movl %%dr2,%%edx\n\
    movzbl %%dh,%%edx\n\
    leal 3(%%edx),%%edx\n\
    leal (%%edx,%%edx,2),%%edx\n\
    leal setxmm(,%%edx,4),%%edx\n\
    movl $.+7,%%esp\n\
    jmp *%%edx\n\
    movb %%ah,%%al\n\
    movl %%dr2,%%edx\n\
    movzbl %%dh,%%edx\n\
    leal 2(%%edx),%%edx\n\
    leal (%%edx,%%edx,2),%%edx\n\
    leal setxmm(,%%edx,4),%%edx\n\
    movl $.+7,%%esp\n\
    jmp *%%edx\n\
    movl %%dr1,%%edx\n\
    movl %%dr3,%%esp\n\
    jmp *%%esp\n\
getxmml:\n\
    movl %%edx,%%dr1\n\
    movl %%esp,%%dr3\n\
    movl %%dr2,%%edx\n\
    movzbl %%dh,%%edx\n\
    leal 2(%%edx),%%edx\n\
    leal getxmm(,%%edx,8),%%edx\n\
    movl $.+7,%%esp\n\
    jmp *%%edx\n\
    movb %%al,%%ah\n\
    movl %%dr2,%%edx\n\
    movzbl %%dh,%%edx\n\
    leal 3(%%edx),%%edx\n\
    leal getxmm(,%%edx,8),%%edx\n\
    movl $.+7,%%esp\n\
    jmp *%%edx\n\
    bswapl %%eax\n\
    movl %%dr2,%%edx\n\
    movzbl %%dh,%%edx\n\
    leal 1(%%edx),%%edx\n\
    leal getxmm(,%%edx,8),%%edx\n\
    movl $.+7,%%esp\n\
    jmp *%%edx\n\
    movb %%al,%%ah\n\
    movl %%dr2,%%edx\n\
    movzbl %%dh,%%edx\n\
    leal getxmm(,%%edx,8),%%edx\n\
    movl $.+7,%%esp\n\
    jmp *%%edx\n\
    movl %%dr1,%%edx\n\
    movl %%dr3,%%esp\n\
    jmp *%%esp\n"
    );

    while(!feof(f))
    {
        *buf=0;
        fgets(buf,sizeof(buf)-1,f);
        chomp(buf);len=strlen(buf);
#if 1
        if (!strncmp(buf,"#APP",4))
        {
            inapp=1;//Skip application asm("") the application is responsable for not use esp or xmm, except for given the control.
        }
        if (!strncmp(buf,"#NO_APP",7))
        {
            inapp=0;
        }
        if (inapp)
        {
            fprintf(w,"%s\n",buf);
            continue;
        }
#endif

	fprintf(w,"#%s #ORIG\n", buf);

	/* handle pushl xxx(%esp) */
        if (regexp("^pushl\\s+(\\d*)\\(\\%esp\\)",buf,len,find)) {
            fprintf(w,"movl %%eax,%%dr0 #NOSTACK\n");
            fprintf(w,"movl %%dr2,%%eax #NOSTACK\n");
            fprintf(w,"movzbl %%al,%%eax #NOSTACK\n");
            fprintf(w,"movb %%al,%%ah #NOSTACK\n");
            fprintf(w,"leal %d(%%eax),%%eax #NOSTACK\n",256*atoi(find[1]));
            fprintf(w,"movl %%eax,%%dr2 #NOSTACK\n");
            fprintf(w,"movl %%dr0,%%eax #NOSTACK\n");
            fprintf(w,"movl $.+10,%%esp\n");
            fprintf(w,"jmp getxmml\n");
            fprintf(w,"movl $.+10,%%esp\n");
            fprintf(w,"jmp dopush\n");
            fprintf(w,"movl %%dr0,%%eax #NOSTACK\n");
            continue;
	}

	/* leal is tricky */
	if (regexp("^leal\\s+(\\d*)\\(\\%esp\\)\\s*,\\s*(.*)$",buf,len,find))
	{
		fprintf(w,"movl %%dr2,%%esp #NOSTACK\n");
		fprintf(w,"%s\n",buf);
		fprintf(w,"movl %%esp,%%dr2 #NOSTACK\n");
		continue;
	}

        if (regexp("^(\\w+)\\s+(.*)\\s*,\\s*(\\d*)\\(\\%esp\\)$",buf,len,find))
        {
            fprintf(w,"movl %%eax,%%dr0 #NOSTACK\n");
            fprintf(w,"movl %%dr2,%%eax #NOSTACK\n");
            fprintf(w,"movzbl %%al,%%eax #NOSTACK\n");
            fprintf(w,"movb %%al,%%ah #NOSTACK\n");
            fprintf(w,"leal %d(%%eax),%%eax #NOSTACK\n",256*atoi(find[3]));
            fprintf(w,"movl %%eax,%%dr2 #NOSTACK\n");
            fprintf(w,"movl %%dr0,%%eax #NOSTACK\n");
            fprintf(w,"movl $.+10,%%esp\n");
            switch(instrlen(find[1],0))
            {
                case 1:fprintf(w,"jmp getxmmb\n");break;
                case 2:fprintf(w,"jmp getxmmw\n");break;
                case 4:fprintf(w,"jmp getxmml\n");break;
            }
            if (!strcmp(find[2],"%eax")||!strcmp(find[2],"%ax")||!strcmp(find[2],"%al"))
            {
                fprintf(w,"movl %%edx,%%esp #NOSTACK\n");
                fprintf(w,"movl %%dr0,%%edx #NOSTACK\n");
                switch(instrlen(find[1],1))
                {
                    case 1:fprintf(w,"%s %%dl,%s\n",find[1],find[2]);break;
                    case 2:fprintf(w,"%s %%dx,%s\n",find[1],find[2]);break;
                    case 4:fprintf(w,"%s %%edx,%s\n",find[1],find[2]);break;
                }
                fprintf(w,"movl %%esp,%%edx #NOSTACK\n");
            }
            else
            {
                switch(instrlen(find[1],1))
                {
                    case 1:fprintf(w,"%s %s,%%al\n",find[1],find[2]);break;
                    case 2:fprintf(w,"%s %s,%%ax\n",find[1],find[2]);break;
                    case 4:fprintf(w,"%s %s,%%eax\n",find[1],find[2]);break;
                }
            }
            fprintf(w,"movl $.+10,%%esp\n");
            switch(instrlen(find[1],1))
            {
                case 1:fprintf(w,"jmp setxmmb\n");break;
                case 2:fprintf(w,"jmp setxmmw\n");break;
                case 4:fprintf(w,"jmp setxmml\n");break;
            }
            fprintf(w,"movl %%dr0,%%eax #NOSTACK\n");
            continue;
        }
        if (regexp("^(\\w+)\\s+(\\d*)\\(\\%esp\\)\\s*,\\s*(.*)$",buf,len,find))
        {
            fprintf(w,"movl %%eax,%%dr0 #NOSTACK\n");
            fprintf(w,"movl %%dr2,%%eax #NOSTACK\n");
            fprintf(w,"movzbl %%al,%%eax #NOSTACK\n");
            fprintf(w,"movb %%al,%%ah #NOSTACK\n");
            fprintf(w,"leal %d(%%eax),%%eax #NOSTACK\n",256*atoi(find[2]));
            fprintf(w,"movl %%eax,%%dr2 #NOSTACK\n");
            fprintf(w,"movl %%dr0,%%eax #NOSTACK\n");
            fprintf(w,"movl $.+10,%%esp\n");
            switch(instrlen(find[1],0))
            {
                case 1:fprintf(w,"jmp getxmmb\n");break;
                case 2:fprintf(w,"jmp getxmmw\n");break;
                case 4:fprintf(w,"jmp getxmml\n");break;
            }
            switch(instrlen(find[1],0))
            {
                case 1:fprintf(w,"%s %%al,%s\n",find[1],find[3]);break;
                case 2:fprintf(w,"%s %%ax,%s\n",find[1],find[3]);break;
                case 4:fprintf(w,"%s %%eax,%s\n",find[1],find[3]);break;
            }
            if (strcmp(find[3],"%eax")&&strcmp(find[3],"%ax")&&strcmp(find[3],"%al"))
                //Nota, nomÃ©s pot ser un registre, (tot ha de ser const, ni malloc's ni res)
                fprintf(w,"movl %%dr0,%%eax #NOSTACK\n");
            continue;
        }
        if (regexp("^(\\w+)\\s+(\\d*)\\(\\%esp\\)$",buf,len,find))
        {
            fprintf(w,"movl %%eax,%%dr0 #NOSTACK\n");
            fprintf(w,"movl %%dr2,%%eax #NOSTACK\n");
            fprintf(w,"movzbl %%al,%%eax #NOSTACK\n");
            fprintf(w,"movb %%al,%%ah #NOSTACK\n");
            fprintf(w,"leal %d(%%eax),%%eax #NOSTACK\n",256*atoi(find[2]));
            fprintf(w,"movl %%eax,%%dr2 #NOSTACK\n");
            fprintf(w,"movl %%dr0,%%eax #NOSTACK\n");
            fprintf(w,"movl $.+10,%%esp\n");
            switch(instrlen(find[1],0))
            {
                case 1:fprintf(w,"jmp getxmmb\n");break;
                case 2:fprintf(w,"jmp getxmmw\n");break;
                case 4:fprintf(w,"jmp getxmml\n");break;
            }
            switch(instrlen(find[1],1))
            {
                case 1:fprintf(w,"%s %%al\n",find[1]);break;
                case 2:fprintf(w,"%s %%ax\n",find[1]);break;
                case 4:fprintf(w,"%s %%eax\n",find[1]);break;
            }
            fprintf(w,"movl $.+10,%%esp\n");
            switch(instrlen(find[1],1))
            {
                case 1:fprintf(w,"jmp setxmmb\n");break;
                case 2:fprintf(w,"jmp setxmmw\n");break;
                case 4:fprintf(w,"jmp setxmml\n");break;
            }
            fprintf(w,"movl %%dr0,%%eax #NOSTACK\n");
            continue;
        }
        if (regexp("\\%esp",buf,len,find))
        {
            fprintf(w,"movl %%dr2,%%esp #NOSTACK\n");
            fprintf(w,"%s\n",buf);
            fprintf(w,"movl %%esp,%%dr2 #NOSTACK\n");
            continue;
        }
        if (regexp("^pushl\\s+(.*)$",buf,len,find))
        {
            fprintf(w,"movl %%eax,%%dr0 #NOSTACK\n");
            fprintf(w,"movl %s,%%eax\n",find[1]);
            fprintf(w,"movl $.+10,%%esp\n");
            fprintf(w,"jmp dopush\n");
            fprintf(w,"movl %%dr0,%%eax #NOSTACK\n");
            continue;
        }
        if (regexp("^popl\\s+(.*)$",buf,len,find))
        {

            fprintf(w,"movl $.+10,%%esp\n");
            fprintf(w,"jmp dopop\n");
            fprintf(w,"movl %%eax,%s\n",find[1]);
            if (strcmp(find[1],"%eax"))
                fprintf(w,"movl %%dr0,%%eax #NOSTACK\n");
            continue;
        }
        if (regexp("^call\\s+(.*)$",buf,len,find))
        {
            fprintf(w,"movl $.+10,%%esp\n");
            fprintf(w,"jmp docall\n");
            fprintf(w,"movl %%dr0,%%eax #NOSTACK\n");
            fprintf(w,".byte 0xe9\n.long %s-4-. #NOSTACK\n",find[1]);//jmp, not works with .global??? gcc???
            continue;
        }
        if (regexp("^(rep\\s*;\\s*|)ret$",buf,len,find))
        {
            fprintf(w,"movl %%eax,%%dr0 #NOSTACK\n");
            fprintf(w,"movl %%dr2,%%eax #NOSTACK\n");
            fprintf(w,"movzbl %%al,%%eax #NOSTACK\n");
            fprintf(w,"movb %%al,%%ah #NOSTACK\n");
            fprintf(w,"leal 4(%%eax),%%eax #NOSTACK\n");
            fprintf(w,"movl %%eax,%%dr2 #NOSTACK\n");
            fprintf(w,"movl $.+10,%%esp\n");
            fprintf(w,"jmp getxmml\n");
            fprintf(w,"movl %%eax,%%esp\n");
            fprintf(w,"movl %%dr0,%%eax #NOSTACK\n");
            fprintf(w,"jmp *%%esp #NOSTACK\n");

            continue;
        }
        fprintf(w,"%s\n",buf);
    }
    fclose(f);
    fclose(w);

    return 0;
}
