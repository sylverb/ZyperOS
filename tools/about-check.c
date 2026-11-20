/* SPDX-License-Identifier: GPL-3.0-only */
#include "../src/about.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int fails;
#define CK(c,s) do { if (!(c)) { fprintf(stderr,"FAIL: %s\n",s); fails++; } } while (0)
static void put(const char *dir, const char *name, const char *text)
{
	char path[512]; FILE *f;
	snprintf(path,sizeof path,"%s/%s",dir,name);
	f=fopen(path,"w"); if(!f) exit(2); fputs(text,f); fclose(f);
}
int main(void)
{
	about_info a = {0}; about_values b; menu_row r[ABOUT_DETAIL_ROWS];
	char dir[]="/tmp/zyperos-about-XXXXXX", p[512];
	a.learned=-1; /* a gauge that does not say whether it measured */
	CK(about_storage_counts(&a,1024,30817792,9068000,9068000),"card counts accepted");
	a.full_uah=a.design_uah=1730000;
	CK(about_detail_rows(&a,r,&b)==6,"six detail rows");
	CK(!strcmp(r[0].value,"22.3 GB"),"used storage uses decimal GB");
	CK(!strcmp(r[1].value,"9.3 GB"),"available storage uses decimal GB");
	CK(!strcmp(r[2].value,"31.6 GB"),"total storage uses decimal GB");
	CK(!strcmp(r[3].value,"100% estimate"),"health clearly says estimate");
	CK(!strcmp(r[4].value,"1,730 mAh")&&!strcmp(r[5].value,"1,730 mAh"),"capacity units and grouping");
	for(int i=0;i<6;i++) CK(!r[i].live,"detail rows are read-only");
	a.full_uah=1384000; about_detail_rows(&a,r,&b);
	CK(!strcmp(r[3].value,"80% estimate"),"health uses full capacity, not charge now");
	a.full_uah=1900000; about_detail_rows(&a,r,&b);
	CK(!strcmp(r[3].value,"100% estimate")&&!strcmp(r[4].value,"1,900 mAh"),"above-design capacity stays visible without health above100");
	a.design_uah=0; about_detail_rows(&a,r,&b);
	CK(!strcmp(r[3].value,"Unknown")&&!strcmp(r[5].value,"Unknown"),"missing design capacity gives no health claim");
	a.full_uah=0;a.design_uah=1730000;about_detail_rows(&a,r,&b);
	CK(!strcmp(r[3].value,"Unknown")&&!strcmp(r[4].value,"Unknown"),"missing full capacity gives no health claim");
	CK(about_storage_counts(&a,4096,100,40,30)&&a.free_bytes==30*4096&&a.used_bytes==60*4096,"reserved blocks are not free or used data");
	CK(!about_storage_counts(&a,4096,UINT64_MAX,0,0)&&!a.storage_known,"overflow rejected");
	about_detail_rows(&a,r,&b); CK(!strcmp(r[0].value,"Unknown")&&!strcmp(r[1].value,"Unknown")&&!strcmp(r[2].value,"Unknown"),"invalid storage clears all rows");
	CK(!about_storage_counts(&a,4096,10,11,9),"free above total rejected");
	CK(!about_storage_counts(&a,4096,10,5,6),"available above free rejected");
	CK(!about_storage_counts(&a,0,10,5,5),"zero block size rejected");
	CK(!about_storage_counts(&a,4096,0,0,0),"zero total is unknown");
	CK(about_storage_counts(&a,1,UINT64_MAX,0,0),"maximum representable storage accepted");
	about_detail_rows(&a,r,&b); CK(strstr(r[2].value," GB")!=NULL,"large storage formats safely");
	if(!mkdtemp(dir))return 2;
	put(dir,"charge_full","1730000\n");put(dir,"charge_full_design","1730000\n");
	about_read(&a,dir,dir); CK(a.storage_known&&a.full_uah==1730000&&a.design_uah==1730000,"filesystem and sysfs fixture read");
	const char *bad[]={"-1\n","0\n","garbage\n","1730000junk\n","18446744073709551616\n",""};
	for(size_t i=0;i<sizeof bad/sizeof bad[0];i++) {
		put(dir,"charge_full",bad[i]);about_read(&a,dir,dir);
		about_detail_rows(&a,r,&b);CK(!strcmp(r[3].value,"Unknown"),"malformed capacity cannot claim health");
	}
	/* A string parser must not accept a valid prefix before an embedded NUL. */
	{
		const char bytes[] = "1730000\0junk\n";
		FILE *f;
		snprintf(p,sizeof p,"%s/charge_full",dir);
		f=fopen(p,"wb");if(!f)return 2;
		fwrite(bytes,1,sizeof bytes-1,f);fclose(f);
		about_read(&a,dir,dir);about_detail_rows(&a,r,&b);
		CK(!strcmp(r[3].value,"Unknown")&&!strcmp(r[4].value,"Unknown"),"embedded NUL cannot claim health");
	}
	put(dir,"charge_full","1730000\n");
	snprintf(p,sizeof p,"%s/charge_full_design",dir);unlink(p);
	about_read(&a,dir,dir);about_detail_rows(&a,r,&b);
	CK(a.full_uah==1730000&&!a.design_uah&&!strcmp(r[3].value,"Unknown"),"removed design reading clears stale health");
	put(dir,"charge_full_design","1730000\n");
	snprintf(p,sizeof p,"%s/charge_full",dir);unlink(p);
	about_read(&a,dir,dir);about_detail_rows(&a,r,&b);
	CK(!a.full_uah&&a.design_uah==1730000&&!strcmp(r[3].value,"Unknown"),"removed full reading clears stale health");
	put(dir,"charge_full"," 1730500 \n");about_read(&a,dir,dir);about_detail_rows(&a,r,&b);
	CK(!strcmp(r[4].value,"1,731 mAh"),"capacity rounds to nearest mAh");
	/* The Pixel 2's gauge says how many times it measured the full charge
	 * (charge_full_learned): never, and the full charge is the design's guess */
	put(dir,"charge_full","1730000\n");put(dir,"charge_full_learned","0\n");
	about_read(&a,dir,dir);about_detail_rows(&a,r,&b);
	CK(a.learned==0&&!strcmp(r[3].value,"Not measured yet")&&!strcmp(r[4].value,"Not measured yet")&&!strcmp(r[5].value,"1,730 mAh"),
	   "a full charge never measured says so, for health and full charge, not 100%");
	put(dir,"charge_full_learned","2\n");about_read(&a,dir,dir);about_detail_rows(&a,r,&b);
	CK(a.learned==2&&!strcmp(r[3].value,"100% estimate")&&!strcmp(r[4].value,"1,730 mAh"),"measured as new: 100%");
	put(dir,"charge_full","1400000\n");put(dir,"charge_full_learned","3\n");
	about_read(&a,dir,dir);about_detail_rows(&a,r,&b);
	CK(!strcmp(r[3].value,"81% estimate")&&!strcmp(r[4].value,"1,400 mAh"),"measured worn: 1400 of 1730 mAh is 81%");
	put(dir,"charge_full_learned","0\n");about_read(&a,dir,dir);about_detail_rows(&a,r,&b);
	CK(!strcmp(r[3].value,"81% estimate"),"a full charge under the design's is a measure even with no count (an older kernel cleared it)");
	put(dir,"charge_full","1730000\n");put(dir,"charge_full_learned","junk\n");
	about_read(&a,dir,dir);about_detail_rows(&a,r,&b);
	CK(a.learned==-1&&!strcmp(r[3].value,"100% estimate"),"an unreadable count is a gauge that does not say");
	snprintf(p,sizeof p,"%s/charge_full_learned",dir);unlink(p);
	about_read(&a,dir,dir);about_detail_rows(&a,r,&b);
	CK(a.learned==-1&&!strcmp(r[3].value,"100% estimate"),"no count (the Brick, an older kernel): the estimate as before");
	put(dir,"charge_full_learned","0\n");
	snprintf(p,sizeof p,"%s/charge_full_design",dir);unlink(p);
	about_read(&a,dir,dir);about_detail_rows(&a,r,&b);
	CK(!strcmp(r[3].value,"Unknown")&&!strcmp(r[5].value,"Unknown"),"never measured with no design reading: unknown");
	put(dir,"charge_full_design","1730000\n");
	snprintf(p,sizeof p,"%s/charge_full_learned",dir);unlink(p);
	about_read(&a,"/path/that/does/not/exist/zyperos-about",NULL);
	CK(!a.storage_known&&!a.full_uah&&!a.design_uah&&a.learned==-1,"missing sources clear old data");
	snprintf(p,sizeof p,"%s/charge_full",dir);unlink(p);
	snprintf(p,sizeof p,"%s/charge_full_design",dir);unlink(p);rmdir(dir);
	if(!fails)puts("ok: About storage, battery estimates, the full charge not measured yet and missing readings");
	return fails?1:0;
}
