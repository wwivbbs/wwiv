/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
/*             Copyright (C)1998-2014, WWIV Software Services             */
/*                                                                        */
/*    Licensed  under the  Apache License, Version  2.0 (the "License");  */
/*    you may not use this  file  except in compliance with the License.  */
/*    You may obtain a copy of the License at                             */
/*                                                                        */
/*                http://www.apache.org/licenses/LICENSE-2.0              */
/*                                                                        */
/*    Unless  required  by  applicable  law  or agreed to  in  writing,   */
/*    software  distributed  under  the  License  is  distributed on an   */
/*    "AS IS"  BASIS, WITHOUT  WARRANTIES  OR  CONDITIONS OF ANY  KIND,   */
/*    either  express  or implied.  See  the  License for  the specific   */
/*    language governing permissions and limitations under the License.   */
/*                                                                        */
/**************************************************************************/
#include "wwivinit.h"


char *tokens[] = 
{
    "FILE:",
    "NAME:",
    "INIT:",
    "SETU:",
    "CONF:",
    "ANSR:",
    "PICK:",
    "HANG:",
    "DIAL:",
    "SEPR:",
    "DEFL:",
    "RESL:",
    "NOTE:",
    "AUTO:",
    "MS=",
    "CS=",
    "AS=",
    "EC=",
    "DC=",
    "FC=",
    "NORM",
    "RINGING",
    "RING",
    "ERR",
    "DIS",
    "CON",
    "NDT",
    "FAX",
    "CID_NUM",
    "CID_NAME",
    "\"",
    "\'",
    NULL
};

typedef enum 
{
    tok_file=0,
    tok_name,
    tok_init,
    tok_setu,
    tok_conf,
    tok_ansr,
    tok_pick,
    tok_hang,
    tok_dial,
    tok_sepr,
    tok_defl,
    tok_resl,
    tok_note,
    tok_auto,
    tok_ms,
    tok_cs,
    tok_as,
    tok_ec,
    tok_dc,
    tok_fc,
    tok_norm,
    tok_ringing,
    tok_ring,
    tok_err,
    tok_dis,
    tok_con,
    tok_ndt,
    tok_fax,
    tok_cid_num,
    tok_cid_name,
    tok_quote,
    tok_sing_quote,
    tok_eol,
    tok_inval
} tok_val;


char *curl;
int curline=0;

void skip_ws()
{
    while ((*curl==' ') || (*curl=='\t') || (*curl=='\n'))
    {
        ++curl;
    }
}


void skip_num()
{
    while ((*curl>='0') && (*curl<='9'))
    {
        ++curl;
    }
}

tok_val get_tok()
{
    char **ss=tokens;
    tok_val t=tok_file;

    skip_ws();

    if (!(*curl))
    {
        return tok_eol;
    }

    while (*ss) 
    {
        if (strncmp(*ss, curl, strlen(*ss))==0) 
        {
            curl += strlen(*ss);
            return t;
        } 
        else 
        {
            ++ss;
            t = static_cast<tok_val>(((int) t) + 1);
        }
    }
    return tok_inval;
}


void grab_string(char *t, int len)
{

    while ((*curl==' ') || (*curl=='\t') || (*curl=='\n'))
    {
        ++curl;
    }

    while ((*curl) && (*curl!='\'') && (*curl!='\"') && (len>0)) 
    {
        if (*curl=='~') 
        {
            *t++=(char)255;
            len--;
        } 
        else if (*curl=='{') 
        {
            *t++='\r';
            len--;
        } 
        else if (*curl=='^') 
        {
            ++curl;
            if ((*curl>='A') && (*curl<='Z')) 
            {
                *t++=*curl-'A'+1;
                len--;
            } 
            else if ((*curl>='a') && (*curl<='z')) 
            {
                *t++=*curl-'a'+1;
                len--;
            }
        } 
        else if (*curl=='\\') 
        {
            ++curl;
            if (*curl) 
            {
                *t++=*curl;
                len--;
            }
        } 
        else 
        {
            *t++=*curl;
            len--;
        }
        if (*curl)
        {
            ++curl;
        }
    }

    while ((*curl) && (*curl!='\'') && (*curl!='\"'))
    {
        ++curl;
    }

    if (curl)
    {
        ++curl;
    }

    *t=0;
}


void grab_result(result_info *r)
{
    tok_val t;

    memset(r, 0, sizeof(result_info));
    r->flag_mask = 0xffff;

    do 
    {
        t=get_tok();
        switch(t) 
        {
        case tok_ms:
            r->modem_speed = (unsigned int) atol(curl);
            skip_num();
            break;
        case tok_cs:
            r->com_speed = (unsigned int) atol(curl);
            skip_num();
            break;
        case tok_as:
            r->flag_mask &= ~flag_as;
            if (*curl=='Y') 
            {
                r->flag_value |= flag_as;
            } 
            else if (*curl!='N') 
            {
                Printf("Unknown flag value 'AS=%c' in line %d\n\n",*curl,curline);
            }
            ++curl;
            break;
        case tok_ec:
            r->flag_mask &= ~flag_ec;
            if (*curl=='Y') 
            {
                r->flag_value |= flag_ec;
            } 
            else if (*curl!='N') 
            {
                Printf("Unknown flag value 'EC=%c' in line %d\n\n",*curl,curline);
            }
            ++curl;
            break;
        case tok_dc:
            r->flag_mask &= ~flag_dc;
            if (*curl=='Y') 
            {
                r->flag_value |= flag_dc;
            } 
            else if (*curl!='N') 
            {
                Printf("Unknown flag value 'DC=%c' in line %d\n\n",*curl,curline);
            }
            ++curl;
            break;
        case tok_fc:
            r->flag_mask &= ~flag_fc;
            if (*curl=='Y') 
            {
                r->flag_value |= flag_fc;
            } 
            else if (*curl!='N') 
            {
                Printf("Unknown flag value 'FC=%c' in line %d\n\n",*curl,curline);
            }
            ++curl;
            break;
        case tok_norm:
            r->main_mode = mode_norm;
            break;
        case tok_ring:
            r->main_mode = mode_ring;
            break;
        case tok_ringing:
            r->main_mode = mode_ringing;
            break;
        case tok_err:
            r->main_mode = mode_err;
            break;
        case tok_dis:
            r->main_mode = mode_dis;
            break;
        case tok_con:
            r->main_mode = mode_con;
            break;
        case tok_ndt:
            r->main_mode = mode_ndt;
            break;
        case tok_fax:
            r->main_mode = mode_fax;
            break;
        case tok_cid_num:
            r->main_mode = mode_cid_num;
            break;
        case tok_cid_name:
            r->main_mode = mode_cid_name;
            break;
        case tok_quote:
            if (r->result[0]) 
            {
                grab_string(r->description,30);
                r->flag_value &= (~flag_append);
            } 
            else 
            {
                grab_string(r->result,40);
                _strupr(r->result);
            }
            break;
        case tok_sing_quote:
            if (r->result[0]) 
            {
                grab_string(r->description,30);
                r->flag_value |= flag_append;
            } 
            else 
            {
                grab_string(r->result,40);
                strupr(r->result);
            }
            break;
        case tok_eol:
            break;
        default:
            t=tok_inval;
            Printf("Unknown token in line %d:\n%s\n\n",curline,curl);
            break;
        }
    } while ((t!=tok_inval) && (t!=tok_eol));
}


FILE *open_modem(char *fn, int *sof)
{
    char s[255], s1[10], s2[10];

    curline=0;

    FILE* pFile = fopen(fn,"r");
    if ( pFile ) 
    {
        *sof=0;
        return pFile;
    }

    strcpy(s,fn);
    char *ss = strrchr( s, '\\' );
    if (ss)
    {
        ++ss;
    }
    else
    {
        ss=s;
    }
    strncpy(s1,ss,8);
    *ss=0;

    s1[8]=0;
    ss=strchr(s1,'.');
    if (ss)
    {
        *ss=0;
    }

    strcat(s,"MODEMS.MDM");
    pFile=fopen(s,"r");
    if (pFile) 
    {
        while (fgets(s,255,pFile)) 
        {
            ++curline;
            if ((s[0]) && (s[0]!='#') && (s[0]!='\n')) 
            {
                curl=s;
                if (get_tok()==tok_file) 
                {
                    if (get_tok() == tok_quote) 
                    {
                        grab_string(s2,8);
                        if (stricmp(s1,s2)==0) 
                        {
                            *sof=1;
                            return pFile;
                        }
                    }
                }
            }
        }
    }

    return NULL;
}


int compile(char *infn, char *outfn, char *notes, int notelen, char *conf, unsigned *pnModemBaud)
{
    char s[256];
    int num_res=0;
    modem_info *mi;
    int i, sof;

    *notes=0;
    *pnModemBaud=0;
    conf[0]=0;

    FILE *pFile = open_modem(infn,&sof);
    if (!pFile)
    {
        return 0;
    }

    int cont = 1;
    while (cont && fgets(s, 255, pFile)) 
    {
        if (strncmp(s,"RESL",4)==0)
        {
            ++num_res;
        }
        if (sof && (strncmp(s,"FILE:",5)==0))
        {
            cont=0;
        }
    }
    fclose( pFile );

    i=sizeof(modem_info) + num_res * sizeof(result_info);
    mi = (modem_info *) bbsmalloc(i);
    memset(mi, 0, i);
    mi->ver=1;

    pFile=open_modem(infn,&sof);
    if (!pFile) 
    {
        return 0;
    }

    cont=1;
    while (cont && fgets(s,255,pFile)) 
    {
        ++curline;
        if ((s[0]) && (s[0]!='#') && (s[0]!='\n')) 
        {
            curl=s;
            switch(get_tok()) 
            {
            case tok_file:
                if (sof) 
                {
                    cont=0;
                }
                break;
            case tok_name:
                if (get_tok() != tok_quote) 
                {
                    Printf("Expected quote on line %d:%s\n\n",curline,s);
                    break;
                }
                grab_string(mi->name,80);
                break;
            case tok_init:
                if (get_tok() != tok_quote) 
                {
                    Printf("Expected quote on line %d:%s\n\n",curline,s);
                    break;
                }
                grab_string(mi->init,160);
                break;
            case tok_setu:
                if (get_tok() != tok_quote) 
                {
                    Printf("Expected quote on line %d:%s\n\n",curline,s);
                    break;
                }
                grab_string(mi->setu,160);
                break;
            case tok_conf:
                if (get_tok() != tok_quote) 
                {
                    Printf("Expected quote on line %d:%s\n\n",curline,s);
                    break;
                }
                grab_string(conf,160);
                break;
            case tok_ansr:
                if (get_tok() != tok_quote) 
                {
                    Printf("Expected quote on line %d:%s\n\n",curline,s);
                    break;
                }
                grab_string(mi->ansr,80);
                break;
            case tok_pick:
                if (get_tok() != tok_quote) 
                {
                    Printf("Expected quote on line %d:%s\n\n",curline,s);
                    break;
                }
                grab_string(mi->pick,80);
                break;
            case tok_hang:
                if (get_tok() != tok_quote) 
                {
                    Printf("Expected quote on line %d:%s\n\n",curline,s);
                    break;
                }
                grab_string(mi->hang,80);
                break;
            case tok_note:
                if (get_tok() != tok_quote) 
                {
                    Printf("Expected quote on line %d:%s\n\n",curline,s);
                    break;
                }
                if (notes) 
                {
                    grab_string(notes+strlen(notes), notelen-strlen(notes));
                }
                break;
            case tok_dial:
                if (get_tok() != tok_quote) 
                {
                    Printf("Expected quote on line %d:%s\n\n",curline,s);
                    break;
                }
                grab_string(mi->dial,80);
                break;
            case tok_sepr:
                if (get_tok() != tok_quote) 
                {
                    Printf("Expected quote on line %d:%s\n\n",curline,s);
                    break;
                }
                grab_string(mi->sepr,9);
                break;
            case tok_defl:
                grab_result(&(mi->defl));
                break;
            case tok_resl:
                grab_result(&(mi->resl[mi->num_resl]));
                ++(mi->num_resl);
                break;
            default:
                Printf("Unknown line %d:\n%s\n\n",curline,s);
                break;
            }
        }
    }
    fclose(pFile);

    pFile=fopen(outfn,"wb");
    fwrite(mi, 1, i, pFile);
    fclose(pFile);
    *pnModemBaud=mi->defl.com_speed;
    BbsFreeMemory(mi);
    return 1;
}


/****************************************************************************/

int fcmp( const void *a, const void *b )
{
    char **aa = ( char ** ) a;
    char **bb = ( char ** ) b;

    return strcmp( *aa,*bb );
}


/****************************************************************************/

void get_descriptions(char *pth, char ***descrs, int *n, autosel_data **ad, int *nn)
{
  int num=0,i,dup, autosel=0;
  char **ss,*ss1;
  char s[255],s1[255],s2[81],s3[81];
  FILE *pFile;
  autosel_data *asd;

  sprintf(s,"%s*.MDM", pth);

  WFindFile fnd;
  fnd.open(s, 0);
  while (fnd.next()) 
  {
    if (stricmp(fnd.GetFileName(),"MODEMS.MDM"))
	{
      ++num;
	}
  }

  sprintf(s1,"%sMODEMS.MDM",pth);
  pFile=fopen(s1,"r");
  if (pFile) {
    while (fgets(s1,100,pFile)) {
      if (strncmp(s1,"FILE:",5)==0)
        ++num;
      else if (strncmp(s1,"AUTO:",5)==0)
        ++autosel;
    }
    fclose(pFile);
  }


  ss=(char**)bbsmalloc(num*sizeof(char *));
  *descrs=ss;
  *n=num;
  *ad=asd=(autosel_data*) bbsmalloc(autosel*sizeof(autosel_data));
  *nn=autosel;
  autosel=0;


  if (num) 
  {
	num=0;
	WFindFile fnd;
	fnd.open(s, 0);

    while (fnd.next()) 
	{
      if (stricmp(fnd.GetFileName(),"MODEMS.MDM")) 
	  {
        strcpy(s1,"(No description given)");
        sprintf(s,"%s%s",pth,fnd.GetFileName());
        pFile=fopen(s,"r");
        if (pFile) {
          while (fgets(s, 255, pFile)) {
            if (strncmp(s,"NAME:",5)==0) {
              curl=s;
              get_tok();
              get_tok();
              grab_string(s1,60);
              break;
            }
          }
          fclose(pFile);
        }

        strcpy(s2,fnd.GetFileName());
        ss1=strchr(s2,'.');
        if (ss1)
          *ss1=0;
        sprintf(s,"%-8.8s | %.60s",s2, s1);
        *ss=strdup(s);
        ++ss;

        ++num;
      }
    }

    sprintf(s,"%sMODEMS.MDM",pth);
    pFile=fopen(s,"r");
    if (pFile) {
      s2[0]=0;
      while (fgets(s,255,pFile)) {
        if (strncmp(s,"FILE:",5)==0) {
          curl=s; get_tok(); get_tok();
          grab_string(s2,8);
          dup=0;
          for (i=0; i<num; i++) {
            if (strnicmp(s2,(*descrs)[i],strlen(s2))==0)
              dup=1;
          }
          if (dup)
            s2[0]=0;
          strcpy(s1,"(No description given)");
        } else if (strncmp(s,"NAME:",5)==0) {
          curl=s; get_tok(); get_tok();
          grab_string(s1,60);
          if (s2[0]) {
            sprintf(s,"%-8.8s | %.60s",s2, s1);
            *ss=strdup(s);
            ++ss;
            ++num;
            s2[0]=0;
          }
        } else if (strncmp(s,"AUTO:",5)==0) {
          curl=s;
          get_tok();
          skip_ws();
          asd[autosel].frm_state=atoi(curl);
          skip_num();
          skip_ws();
          asd[autosel].to_state=atoi(curl);
          skip_num();
          if (get_tok() != tok_quote) {
            Printf("Error on line: '%s'\n",s);
            break;
          }
          grab_string(s3,8);
          asd[autosel].type = strdup(s3);
          if (get_tok() != tok_quote) {
            Printf("Error on line: '%s'\n",s);
            break;
          }
          grab_string(s3,80);
          asd[autosel].send = strdup(s3);
          if (get_tok() != tok_quote) {
            Printf("Error on line: '%s'\n",s);
            break;
          }
          grab_string(s3,80);
          asd[autosel].recv = strdup(s3);
          autosel++;
        }
      }
      if (s2[0]) {
        sprintf(s,"%-8.8s | %.60s",s2, s1);
        *ss=strdup(s);
        ++ss;
        ++num;
      }
      fclose(pFile);
    }
    *n=num;
    *nn=autosel;
    qsort(*descrs, *n, sizeof(char *), fcmp);

  }
}


/****************************************************************************/


char *scrn;

void hlt(int y, int x, int nc)
{
	for (int x1=x; x1<(x+nc); x1++)
	{
		app->localIO->set_attr_xy(x1, y, 0x1f);
	}
}

void unhlt(int y, int x, int nc)
{
	for (int x1=x; x1<(x+nc); x1++)
	{
		app->localIO->set_attr_xy(x1, y, 0x03);
	}
}


int select_strings(char **strs, int count, int cur, int yt, int yb, int xl, int xr)
{
    int cur_pos = 0;
    unsigned char ch;
    int i;

    int cur_sel = cur;
    int i1=(yb-yt)/2;
    int cur_lin = ( cur > i1 ) ? i1 : cur;

    for (i=yt; i<=yb; i++) 
    {
        app->localIO->LocalGotoXY(xl,i);
        i1=i-yt+cur_sel-cur_lin;
        if ( i1>=0 && i1<count ) 
        {
            app->localIO->LocalPuts(strs[i1]);
        }
    }


    hlt(cur_lin+yt, xl, xr+1);

    for( ;; )
    {
        app->localIO->LocalGotoXY(0,yt-1);
        ch = app->localIO->getchd();
        if ((ch==0) || (ch==224)) 
        {
            ch=app->localIO->getchd();
            switch(ch) 
            {
            case UPARROW: // Up
                if (cur_sel) 
                {
                    unhlt(cur_lin+yt, xl, xr+1);

                    cur_sel--;
                    cur_lin--;
                    if (cur_lin<0) 
                    {
                        app->localIO->LocalScrollScreen( yt, yb, WLocalIO::scrollDown );
                        cur_lin++;
                        app->localIO->LocalGotoXY(xl,cur_lin+yt);
                        app->localIO->LocalPuts(strs[cur_sel]);
                    }

                    hlt(cur_lin+yt, xl, xr+1);
                }
                break;
            case DNARROW: // down
                if (cur_sel < (count-1)) 
                {
                    unhlt(cur_lin+yt, xl, xr+1);

                    cur_sel++;
                    cur_lin++;
                    if (cur_lin > (yb-yt)) 
                    {
                        app->localIO->LocalScrollScreen( yt, yb, WLocalIO::scrollUp );
                        cur_lin--;
                        app->localIO->LocalGotoXY(xl,cur_lin+yt);
                        app->localIO->LocalPuts(strs[cur_sel]);
                    }

                    hlt(cur_lin+yt, xl, xr+1);
                }
                break;
            case PAGEUP: // page up
                unhlt(cur_lin+yt, xl, xr+1);
                for (i=yt; i<=yb; i++) 
                {
                    app->localIO->LocalGotoXY(xl,i);
                    app->localIO->LocalPuts("                                                                               ");
                }
                cur_lin = 0;

                if ((cur_sel - 22) <= 0 )
                {
                    cur_pos = 0;
                }
                else
                {
                    cur_pos = cur_sel - 22;
                }

                for (i=yt; i<=yb; i++) 
                {
                    app->localIO->LocalGotoXY(xl,i);
                    i1=i-yt+cur_pos;
                    app->localIO->LocalPuts(strs[i1]);
                }
                cur_sel = cur_pos ;
                hlt(yt, xl, xr+1);
                break;

            case PAGEDN: // page down
                if (cur_sel + 22 <= count) 
                {
                    unhlt(cur_lin+yt, xl, xr+1);
                    for (i=yt; i<=yb; i++) 
                    {
                        app->localIO->LocalGotoXY(xl,i);
                        app->localIO->LocalPuts("                                                                               ");
                    }
                    cur_lin = 0;
                    cur_pos = cur_sel + 22;
                    cur_sel = cur_pos;

                    for (i=yt; i<=yb; i++) 
                    {
                        app->localIO->LocalGotoXY(xl,i);
                        i1=i-yt+cur_pos;
                        if (i1<count) 
                        {
                            app->localIO->LocalPuts(strs[i1]);
                        }
                    }
                    hlt(cur_lin+yt, xl, xr+1);
                }
                break;
            }
        } 
        else 
        {
            switch( ch ) 
            {
            case RETURN: // return
                return cur_sel;
            case ESC: // escape
                return -1;
            }
        }
    }
}


