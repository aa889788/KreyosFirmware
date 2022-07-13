/** 
*	@file pedometer.c
*
*	@details Description:
*  This file provides basic pedometer algorithm designed for 62.5Hz (16msec) sample rates
*
* @author S. Ravindran
* @author Texas Instruments, Inc
* @version 1.0 - Original release Dec 2011
* @note Built with CCS Version 5.1.0.09000
* 
*/
//Pedometer Source and Object Code Software License Agreement
//
//Important  Read carefully: In this Agreement "you" means you personally if you will exercise the rights granted for your
//own benefit, but it means your company (or you on behalf of your company) if you will exercise the rights granted for your
//companys benefit.  This Source and Object Code Software License Agreement ("Agreement") is a legal agreement
//between you (either an individual or entity) and Texas Instruments Incorporated ("TI").  The "Licensed Materials" subject
//to this Agreement include the software programs and associated electronic documentation (in each case in whole or in
//part) that accompany this Agreement, are set forth in the applicable software manifest and that you access "on-line", as
//well as any updates or upgrades to such software programs and documentation, if any, provided to you at TIs sole
//discretion.  The Licensed Materials are specifically designed and licensed for use solely and exclusively with
//microcontroller devices manufactured by or for TI ("TI Devices").  By installing, copying or otherwise using the Licensed
//Materials you agree to abide by the provisions set forth herein.  This Agreement is displayed for you to read prior to using
//the Licensed Materials.  If you choose not to accept or agree with these provisions, do not download or install the
//Licensed Materials.
//
//Note Regarding Possible Access to Open Source Software:  The Licensed Materials may be bundled with Open
//Source Software.  "Open Source Software" means any software licensed under terms requiring that (A) other software
//("Proprietary Software") incorporated, combined or distributed with such software or developed using such software: (i)
//be disclosed or distributed in source code form; or (ii) otherwise be licensed on terms inconsistent with the terms of this
//Agreement, including but not limited to permitting use of the Proprietary Software on or with devices other than TI
//Devices, or (B) require the owner of Proprietary Software to license any of its patents to users of the Open Source
//Software and/or Proprietary Software incorporated, combined or distributed with such Open Source Software or
//developed using such Open Source Software.
//
//If by accepting this Agreement you gain access to Open Source Software, such Open Source Software will be listed in the
//applicable software manifest.  Your use of the Open Source Software is subject to the separate licensing terms applicable
//to such Open Source Software as specified in the applicable software manifest.  For clarification, this Agreement does not
//limit your rights under, or grant you rights that supersede, the license terms of any applicable Open Source Software
//license agreement.  If any of the Open Source Software has been provided to you in object code only under terms that
//obligate TI to provide to you, or show you where you can access, the source code versions of such Open Source
//Software, TI will provide to you, or show you where you can access, such source code if you contact TI at Texas
//Instruments Incorporated, 12500 TI Boulevard, Mail Station 8638, Dallas, Texas 75243, Attention: Contracts Manager,
//Embedded Processing.  In the event you choose not to accept or agree with the terms in any applicable Open Source
//Software license agreement, you must terminate this Agreement.
//
//
//1.	License Grant and Use Restrictions.
//
//a.	Licensed Materials License Grant.  Subject to the terms of this Agreement, TI hereby grants to you a limited,
//non-transferable, non-exclusive, non-assignable, non-sublicensable, fully paid-up and royalty-free license to:
//
//			i.	Limited Source Code License.  make copies, prepare derivative works, display internally and use internally
//the Licensed Materials provided to you in source code for the sole purpose of developing object and executable
//versions of such Licensed Materials, or any derivative thereof, that execute solely and exclusively on TI Devices,
//for end use in Licensee Products, and maintaining and supporting such Licensed Materials, or any derivative
//thereof, and Licensee Products.  For purposes of this Agreement, "Licensee Product" means a product that
//consists of both hardware, including one or more TI Devices, and software components, including only
//executable versions of the Licensed Materials that execute solely and exclusively on such TI Devices.
//
//			ii.	Object Code Evaluation, Testing and Use License.  make copies, display internally, distribute internally and
//use internally the Licensed Materials in object code for the sole purposes of evaluating and testing the Licensed
//Materials and designing and developing Licensee Products, and maintaining and supporting the Licensee
//Products;
//
//			iii.	Demonstration License.  demonstrate to third parties the Licensed Materials executing solely and exclusively
//on TI Devices as they are used in Licensee Products, provided that such Licensed Materials are demonstrated in
//object or executable versions only and
//
//		iv.	Production and Distribution License.  make, use, import, export and otherwise distribute the Licensed
//Materials as part of a Licensee Product, provided that such Licensee Products include only embedded
//executable copies of such Licensed Materials that execute solely and exclusively on TI Devices.
//
//	b.	Contractors.  The licenses granted to you hereunder shall include your on-site and off-site contractors (either an
//individual or entity), while such contractors are performing work for or providing services to you, provided that such
//contractors have executed work-for-hire agreements with you containing applicable terms and conditions consistent
//with the terms and conditions set forth in this Agreement and provided further that you shall be liable to TI for any
//breach by your contractors of this Agreement to the same extent as you would be if you had breached the
//Agreement yourself.
//
//	c.	No Other License.  Nothing in this Agreement shall be construed as a license to any intellectual property rights of
//TI other than those rights embodied in the Licensed Materials provided to you by TI.  EXCEPT AS PROVIDED
//HEREIN, NO OTHER LICENSE, EXPRESS OR IMPLIED, BY ESTOPPEL OR OTHERWISE, TO ANY OTHER TI
//INTELLECTUAL PROPERTY RIGHTS IS GRANTED HEREIN.
//
//	d.	Covenant not to Sue.  You agree not to assert a claim against TI or its licensees that the Licensed Materials
//infringe your intellectual property rights.
//
//	e.	Restrictions.  You shall maintain the source code versions of the Licensed Materials under password control
//protection and shall not disclose such source code versions of the Licensed Materials, to any person other than your
//employees and contractors whose job performance requires access.  You shall not use the Licensed Materials with a
//processing device other than a TI Device, and you agree that any such unauthorized use of the Licensed Materials is
//a material breach of this Agreement.  You shall not use the Licensed Materials for the purpose of analyzing or
//proving infringement of any of your patents by either TI or TI's customers.  Except as expressly provided in this
//Agreement, you shall not copy, publish, disclose, display, provide, transfer or make available the Licensed Materials
//to any third party and you shall not sublicense, transfer, or assign the Licensed Materials or your rights under this
//Agreement to any third party.  You shall not mortgage, pledge or encumber the Licensed Materials in any way.  You
//may use the Licensed Materials with Open Source Software or with software developed using Open Source Software
//tools provided you do not incorporate, combine or distribute the Licensed Materials in a manner that subjects the
//Licensed Materials to any license obligations or any other intellectual property related terms of any license governing
//such Open Source Software.
//
//	f.	Termination.  This Agreement is effective on the date the Licensed Materials are delivered to you together with
//this Agreement and will remain in full force and effect until terminated.  You may terminate this Agreement at any
//time by written notice to TI.  Without prejudice to any other rights, if you fail to comply with the terms of this
//Agreement or you are acquired, TI may terminate your right to use the Licensed Materials upon written notice to you.
//Upon termination of this Agreement, you will destroy any and all copies of the Licensed Materials in your possession,
//custody or control and provide to TI a written statement signed by your authorized representative certifying such
//destruction. Except for Sections 1(a), 1(b) and 1(d), all provisions of this Agreement shall survive termination of this
//Agreement.
//
//2.	Licensed Materials Ownership.  The Licensed Materials are licensed, not sold to you, and can only be used in
//accordance with the terms of this Agreement.  Subject to the licenses granted to you pursuant to this Agreement, TI
//and its licensors own and shall continue to own all right, title and interest in and to the Licensed Materials, including
//all copies thereof.  You agree that all fixes, modifications and improvements to the Licensed Materials conceived of
//or made by TI that are based, either in whole or in part, on your feedback, suggestions or recommendations are the
//exclusive property of TI and all right, title and interest in and to such fixes, modifications or improvements to the
//Licensed Materials will vest solely in TI.  Moreover, you acknowledge and agree that when your independently
//developed software or hardware components are combined, in whole or in part, with the Licensed Materials, your
//right to use the combined work that includes the Licensed Materials remains subject to the terms and conditions of
//this Agreement.
//
//3.	Intellectual Property Rights.
//
//	a.	The Licensed Materials contain copyrighted material, trade secrets and other proprietary information of TI and its
//licensors and are protected by copyright laws, international copyright treaties, and trade secret laws, as well as other
//intellectual property laws.  To protect TI's and its licensors' rights in the Licensed Materials, you agree, except as
//specifically permitted by statute by a provision that cannot be waived by contract, not to "unlock", decompile, reverse
//engineer, disassemble or otherwise translate to a human-perceivable form any portions of the Licensed Materials
//provided to you in object code format only, nor permit any person or entity to do so.  You shall not remove, alter,
//cover, or obscure any confidentiality, trade secret, trade mark, patent, copyright or other proprietary notice or other
//identifying marks or designs from any component of the Licensed Materials and you shall reproduce and include in
//all copies of the Licensed Materials the copyright notice(s) and proprietary legend(s) of TI and its licensors as they
//appear in the Licensed Materials.  TI reserves all rights not specifically granted under this Agreement.
//
//	b.	Certain Licensed Materials may be based on industry recognized standards or software programs published by
//industry recognized standards bodies and certain third parties may claim to own patents, copyrights, and other
//intellectual property rights that cover implementation of those standards.  You acknowledge and agree that this
//Agreement does not convey a license to any such third party patents, copyrights, and other intellectual property
//rights and that you are solely responsible for any patent, copyright, or other intellectual property right claim that
//relates to your use or distribution of the Licensed Materials or your use or distribution of your products that include or
//incorporate the Licensed Materials.  Moreover, you acknowledge that you are responsible for any fees or royalties
//that may be payable to any third party based on such third party's interests in the Licensed Materials or any
//intellectual property rights that cover implementation of any industry recognized standard, any software program
//published by any industry recognized standards bodies or any other proprietary technology.
//
//4.	Audit Right.  At TI's request, and within thirty (30) calendar days after receiving written notice, you shall permit an
//internal or independent auditor selected by TI to have access, no more than twice each calendar year (unless the
//immediately preceding audit revealed a discrepancy) and during your regular business hours, to all of your
//equipment, records, and documents as may contain information bearing upon the use of the Licensed Materials.
//You shall keep full, complete, clear and accurate records with respect to product sales and distributions for a period
//beginning with the then-current calendar year and going back three (3) years.
//
//5.	Confidential Information.  You acknowledge and agree that the Licensed Materials contain trade secrets and other
//confidential information of TI and its licensors.  You agree to use the Licensed Materials solely within the scope of
//the licenses set forth herein, to maintain the Licensed Materials in strict confidence, to use at least the same
//procedures and degree of care that you use to prevent disclosure of your own confidential information of like
//importance but in no instance less than reasonable care, and to prevent disclosure of the Licensed Materials to any
//third party, except as may be necessary and required in connection with your rights and obligations hereunder;
//provided, however, that you may not provide the Licensed Materials to any business organization or group within
//your company or to customers or contractors that design or manufacture semiconductors unless TI gives written
//consent.  You agree to obtain executed confidentiality agreements with your employees and contractors having
//access to the Licensed Materials and to diligently take steps to enforce such agreements in this respect.  TI may
//disclose your contact information to TI's licensors.
//
//6.	Warranties and Limitations.  THE LICENSED MATERIALS ARE PROVIDED "AS IS".  FURTHERMORE, YOU
//ACKNOWLEDGE AND AGREE THAT THE LICENSED MATERIALS HAVE NOT BEEN TESTED OR CERTIFIED
//BY ANY GOVERNMENT AGENCY OR INDUSTRY REGULATORY ORGANIZATION OR ANY OTHER THIRD
//PARTY ORGANIZATION.  YOU AGREE THAT PRIOR TO USING, INCORPORATING OR DISTRIBUTING THE
//LICENSED MATERIALS IN OR WITH ANY COMMERCIAL PRODUCT THAT YOU WILL THOROUGHLY TEST
//THE PRODUCT AND THE FUNCTIONALITY OF THE LICENSED MATERIALS IN OR WITH THAT PRODUCT AND
//BE SOLELY RESPONSIBLE FOR ANY PROBLEMS OR FAILURES.
//
//You acknowledge and agree that You have unique knowledge concerning the regulatory requirements and safety
//implications of Licensee Products, including the ramifications of using the Licensed Materials in Licensee Products;
//and You have full and exclusive responsibility to assure the safety of Licensee Products and compliance of Licensee
//Products (and of any Licensed Materials and TI Devices therein) with all applicable Food and Drug Administration
//regulations, State and Federal laws and other applicable requirements, notwithstanding any design assistance or
//support that may be provided by TI.
//
//TI AND ITS LICENSORS MAKE NO WARRANTY OR REPRESENTATION, EITHER EXPRESS, IMPLIED OR
//STATUTORY, REGARDING THE LICENSED MATERIALS, INCLUDING BUT NOT LIMITED TO ANY IMPLIED
//WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT
//OF ANY THIRD PARTY PATENTS, COPYRIGHTS, TRADE SECRETS OR OTHER INTELLECTUAL PROPERTY
//RIGHTS.  YOU AGREE TO USE YOUR INDEPENDENT JUDGMENT IN DEVELOPING YOUR PRODUCTS.
//NOTHING CONTAINED IN THIS AGREEMENT WILL BE CONSTRUED AS A WARRANTY OR
//REPRESENTATION BY TI TO MAINTAIN PRODUCTION OF ANY TI SEMICONDUCTOR DEVICE OR OTHER
//HARDWARE OR SOFTWARE WITH WHICH THE LICENSED MATERIALS MAY BE USED.
//
//IN NO EVENT SHALL TI OR ITS LICENSORS, BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL,
//PUNITIVE OR CONSEQUENTIAL DAMAGES, HOWEVER CAUSED, ON ANY THEORY OF LIABILITY, IN
//CONNECTION WITH OR ARISING OUT OF THIS AGREEMENT OR THE USE OF THE LICENSED MATERIALS
//REGARDLESS OF WHETHER TI HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.  EXCLUDED
//DAMAGES INCLUDE, BUT ARE NOT LIMITED TO, COST OF REMOVAL OR REINSTALLATION, OUTSIDE
//COMPUTER TIME, LABOR COSTS, LOSS OF DATA, LOSS OF GOODWILL, LOSS OF PROFITS, LOSS OF
//SAVINGS, OR LOSS OF USE OR INTERRUPTION OF BUSINESS.  IN NO EVENT WILL TI'S OR ITS
//LICENSORS' AGGREGATE LIABILITY UNDER THIS AGREEMENT OR ARISING OUT OF YOUR USE OF THE
//LICENSED MATERIALS EXCEED FIVE HUNDRED U.S. DOLLARS (US$500).
//
//	Because some jurisdictions do not allow the exclusion or limitation of incidental or consequential damages or
//limitation on how long an implied warranty lasts, the above limitations or exclusions may not apply to you.
//
//7.	Indemnification Disclaimer.  YOU ACKNOWLEDGE AND AGREE THAT TI SHALL NOT BE LIABLE FOR AND
//SHALL NOT DEFEND OR INDEMNIFY YOU AGAINST ANY THIRD PARTY INFRINGEMENT CLAIM THAT
//RELATES TO OR IS BASED ON YOUR MANUFACTURE, USE, OR DISTRIBUTION OF THE LICENSED
//MATERIALS OR YOUR MANUFACTURE, USE, OFFER FOR SALE, SALE, IMPORTATION OR DISTRIBUTION OF
//YOUR PRODUCTS THAT INCLUDE OR INCORPORATE THE LICENSED MATERIALS.
//
//8.	No Technical Support.  TI and its licensors are under no obligation to install, maintain or support the Licensed
//Materials.
//
//9.	Notices.  All notices to TI hereunder shall be delivered to Texas Instruments Incorporated, 12500 TI Boulevard, Mail
//Station 8638, Dallas, Texas 75243, Attention: Contracts Manager  Embedded Processing, with a copy to Texas
//Instruments Incorporated, 13588 N. Central Expressway, Mail Station 3999, Dallas, Texas 75243, Attention: Law
//Department  Embedded Processing.  All notices shall be deemed served when received by TI.
//
//10.	Export Control.  The Licensed Materials are subject to export control under the U.S. Commerce Department's Export
//Administration Regulations ("EAR").  Unless prior authorization is obtained from the U.S. Commerce Department, neither
//you nor your subsidiaries shall export, re-export, or release, directly or indirectly (including, without limitation, by
//permitting the Licensed Materials to be downloaded), any technology, software, or software source code, received from
//TI, or export, directly or indirectly, any direct product of such technology, software, or software source code, to any
//person, destination or country to which the export, re-export, or release of the technology, software, or software source
//code, or direct product is prohibited by the EAR.  You represent and warrant that you (i) are not located in, or under the
//control of, a national or resident of Cuba, Iran, North Korea, Sudan and Syria or any other country subject to a U.S.
//goods embargo; (ii) are not on the U.S. Treasury Department's List of Specially Designated Nationals or the U.S.
//Commerce Department's Denied Persons List or Entity List; and (iii) will not use the Licensed Materials or transfer the
//Licensed Materials for use in any military, nuclear, chemical or biological weapons, or missile technology end-uses.  Any
//software export classification made by TI shall not be construed as a representation or warranty regarding the proper
//export classification for such software or whether an export license or other documentation is required for the
//exportation of such software.
//
//11.	Governing Law and Severability; Waiver.  This Agreement will be governed by and interpreted in accordance with
//the laws of the State of Texas, without reference to conflict of laws principles.  If for any reason a court of competent
//jurisdiction finds any provision of the Agreement to be unenforceable, that provision will be enforced to the maximum
//extent possible to effectuate the intent of the parties, and the remainder of the Agreement shall continue in full force
//and effect.  This Agreement shall not be governed by the United Nations Convention on Contracts for the
//International Sale of Goods, or by the Uniform Computer Information Transactions Act (UCITA).  The parties agree
//that non-exclusive jurisdiction for any dispute arising out of or relating to this Agreement lies within the courts located
//in the State of Texas.  Notwithstanding the foregoing, any judgment may be enforced in any United States or foreign
//court, and either party may seek injunctive relief in any United States or foreign court.  Failure by TI to enforce any
//provision of this Agreement shall not be deemed a waiver of future enforcement of that or any other provision in this
//Agreement or any other agreement that may be in place between the parties.
//
//12.	PRC Provisions.  If you are located in the People's Republic of China ("PRC") or if the Licensed Materials will be
//sent to the PRC, the following provisions shall apply:
//
//	a.	Registration Requirements.  You shall be solely responsible for performing all acts and obtaining all approvals
//that may be required in connection with this Agreement by the government of the PRC, including but not limited to
//registering pursuant to, and otherwise complying with, the PRC Measures on the Administration of Software
//Products, Management Regulations on Technology Import-Export, and Technology Import and Export Contract
//Registration Management Rules.  Upon receipt of such approvals from the government authorities, you shall forward
//evidence of all such approvals to TI for its records.  In the event that you fail to obtain any such approval or
//registration, you shall be solely responsible for any and all losses, damages or costs resulting therefrom, and shall
//indemnify TI for all such losses, damages or costs.
//
//b.	Governing Language.  This Agreement is written and executed in the English language and shall be
//authoritative and controlling, whether or not translated into a language other than English to comply with law or for
//reference purposes.  If a translation of this Agreement is required for any purpose, including but not limited to
//registration of the Agreement pursuant to any governmental laws, regulations or rules, you shall be solely
//responsible for creating such translation.
//
//13.	Contingencies.	TI shall not be in breach of this Agreement and shall not be liable for any non-performance or
//delay in performance if such non-performance or delay is due to a force majeure event or other circumstances
//beyond TI's reasonable control.
//
//14.		Entire Agreement.  This is the entire agreement between you and TI and this Agreement supersedes any prior
//agreement between the parties related to the subject matter of this Agreement.  Notwithstanding the foregoing, any
//signed and effective software license agreement relating to the subject matter hereof and stating expressly that such
//agreement shall control regardless of any subsequent click-wrap, shrink-wrap or web-wrap, shall supersede the
//terms of this Agreement.  No amendment or modification of this Agreement will be effective unless in writing and
//signed by a duly authorized representative of TI.  You hereby warrant and represent that you have obtained all
//authorizations and other applicable consents required empowering you to enter into this Agreement.
//TILAW-#282048-v2-MCU Pedometer


/***************************************************
* NOTES:
*  
****************************************************/

#include "b_filter_coeff.h"
#include "pedometer.h"

static void b_filter(signed int *, signed int *, unsigned short);
static void b2_filter(signed int *, signed int *, unsigned short);
void do_q15_mult(signed int, signed int *);

static signed int x[50], y[50], z[50];
static signed int g[38], g_tmp, g_prev;
static signed int vcnt_up=0,vcnt_dn=0;
static signed int x_temp[24], y_temp[24], z_temp[24], g_temp[12];
static unsigned short g_index=0;
static unsigned short step_cnt=0,cnt_flg=1, cnt_trip=46;
static unsigned short cnt=0, prev_cnt=0, no_cnt_flg, disp_trip=0;
static unsigned short first_loop=1,up_flg=1,dn_flg=0;
static unsigned short length_g=14, reset_time=99,pos_flg=1,strt_trip=0;

const unsigned short firmware_version = 0x0100;

unsigned short ped_get_version(void)
{
  return(firmware_version);
}
/**
* @brief <b>Description:</b> Updates pedometer sample every time accelerometer is read
* @param[in] *p_data
* @return result
**/
char ped_update_sample(signed short* p_data)
{
  // WE COME HERE ON EVERY ACCEL SAMPLE
  static unsigned int sample_index = 0;
  unsigned char result = 0;
  
  x[sample_index] = (unsigned int) *p_data++;		// x
  y[sample_index] = (unsigned int) *p_data++;		// y
  z[sample_index] = (unsigned int) *p_data;		//z
  
  sample_index++;
  
  if(sample_index > 49)
  {
    sample_index = 24;
    result = 1;
  }
  return(result);
}

/**
* @brief <b>Description:</b> Initialize pedometer variables and structures
**/
void ped_step_detect_init(void)
{
  unsigned char index;
  for(index=0; index < 50; index++)
  {
    x[index] = 0;	
    y[index] = 0;
    z[index] = 0;
  }
  
  index = 0;
  first_loop = 1;
  step_cnt=0;
  cnt=0;
  prev_cnt=0;
  cnt_flg=1;
  cnt_trip=46;
  disp_trip=0;
  g_index=0;
  reset_time=99;
  length_g=14;
  pos_flg=1;
  vcnt_up=0;
  vcnt_dn=0;
  strt_trip=0;
  up_flg=1;
  dn_flg=0;
  
}

/**
* @brief <b>Description:</b> This is the algorithm that processes real-time data input
* @return step count
**/
unsigned short ped_step_detect(void)
{
  unsigned short i, j;
  // do processing
  b_filter(&x[0], &x_temp[0], first_loop);	   
  b_filter(&y[0], &y_temp[0], first_loop);
  b_filter(&z[0], &z_temp[0], first_loop);
  
  
  for(j=0;j<26;j++)
  {
    if (x[j] < 0)
    {do_q15_mult(0x8000,&x[j]);} // mult by -1
    if (y[j] < 0)
    {do_q15_mult(0x8000,&y[j]);}
    if (z[j] < 0)
    {do_q15_mult(0x8000,&z[j]);}
    g[j+g_index] = x[j]+y[j]+z[j];
  }		
  
  b2_filter(&g[0], &g_temp[0], first_loop);	
  
  if (first_loop==1)
  {
    g_prev=g[0];
  }
  
  for (j=0;j<length_g;j++)
  {
    if (g[j]-g_prev > 0)
    {
      pos_flg=1;
    }
    else if (g[j]-g_prev < 0)
    {
      pos_flg=0;
    }
    
    if (cnt_flg==1 && pos_flg==1)
    {
      if( (cnt_trip>11 && vcnt_up > 91) || (cnt_trip>15 && vcnt_up > 71)||(cnt_trip>19 && vcnt_up > 54) || (cnt_trip>23 && vcnt_up > 41) || (cnt_trip>29 && vcnt_up>33) )
      {
        cnt_flg=0;
        cnt=cnt+1;
        cnt_trip=0;
        vcnt_up=0;
        up_flg=0;
        strt_trip=1;
      }
    }
    else if(cnt_flg==0) 
    {
      if ((cnt_trip>5) && (vcnt_dn > 3)) //15
      {
        cnt_flg=1;
        vcnt_dn=0;
        dn_flg=0;
      }
      
      else if (cnt_trip>59)
      {
        cnt_flg=1;
        vcnt_dn=0;
        dn_flg=0;
      }
    }
    
    if (pos_flg==1 && cnt_flg==1)
    {
      up_flg=1;
    }
    else if (pos_flg==0 && cnt_flg==0)
    {
      dn_flg=1;
      if (strt_trip==1)
      {
        cnt_trip=0;
        strt_trip=0;
        vcnt_up=0;
      }
    }
    
    if (up_flg==1)
    {
      vcnt_up=vcnt_up+(g[j]-g_prev);
    }
    else if (dn_flg==1)
    {
      vcnt_dn=vcnt_dn+(g_prev-g[j]);
    }
    
    if (vcnt_up < 0)
    {
      vcnt_up=0;
      up_flg=0;
    }
    else if (vcnt_dn < 0)
    {
      vcnt_dn=0;
      dn_flg=0;
    }
    
    cnt_trip++;
    g_prev=g[j];
    
    if (prev_cnt==cnt)
    {
      no_cnt_flg=1;
    }
    else
    {
      no_cnt_flg=0;
      prev_cnt=cnt;
    }
    
    if ((no_cnt_flg==1) && (cnt_trip > reset_time))
    {
      disp_trip=0;
      cnt=step_cnt;
      reset_time=99;
    }
  }
  
  if (disp_trip > 15)
  {
    step_cnt=cnt;
    reset_time=85;
  }
  
  if (disp_trip < 16)
    disp_trip++;
  
  
  first_loop=0;	
  length_g=26;
  g_index=12;
  
  return(step_cnt);
  
}

/**
* @brief <b>Description:</b> B filter algorithm
* @param[in] *d
* @param[in] *t
* @param[in] first_loop
**/
static void b_filter(signed int *d, signed int *t, unsigned short first_loop)
{
  unsigned short i,j;
  signed int R=0;
  
  // initialize data
  if(first_loop==1)
  {
    for(i=0;i<24;i++)
    {
      *(t+i)= *(d+26+i);
    }
  }
  else
  {
    for (i=0;i<24;i++)
    {
      *(d+i) = *(t+i);
      *(t+i) = *(d+26+i);
    }
  }
  
  for(j=24;j<50;j++)
  {
    R=0;
    for(i=0;i<25;i++)
    {
      signed int result;
      result = d[j - i];
      do_q15_mult(FILTER_COEFF_b[i], &result); 
      R += result;
    }  
    d[j-24]=R;         
  }
}

/**
* @brief <b>Description: B2 filter algorithm</b>
* @param[in] *d
* @param[in] *t
* @param[in] first_loop
**/
static void b2_filter(signed int *d, signed int *t, unsigned short first_loop)
{
  unsigned short i,j,nn=26;
  signed int R=0;
  
  // initialize data
  if(first_loop==1)
  {
    for(i=0;i<12;i++)
    {
      *(t+i)= *(d+14+i);
    }
  }
  else
  {
    for (i=0;i<12;i++)
    {
      *(d+i) = *(t+i);
      *(t+i) = *(d+26+i);
    }
    nn=38;
  }
  
  for(j=12;j<nn;j++)
  {
    R=0;
    for(i=0;i<13;i++)
    {
      signed int result;
      result = d[j - i];
      do_q15_mult(0x09D8, &result);
      R += result;
    }  
    d[j-12]=R;         
  }
}
