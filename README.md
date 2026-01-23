# NIBTools - Commodore 1541/1571 disk image nibbler

homepage: https://c64preservation.rittwage.com/dp.php?pg=nibtools

NIBTOOLS is copyrighted
(C) 2005 Pete Rittwage and the C64 Preservation Project team

It is based on MNIB which is copyrighted
(C) 2000 Markus Brenner



See in branch "upstream" for the original version.
Use `git fetch origin 'refs/notes/*:refs/notes/*'` 
to get mapping between subversion revisions and git commits.

See in branches "markusC64-*" for an improved version.
Adding of GEOS Copy Protection and Timex V1 Copy Protection for d64 to 
g64 conversion has been added.
Also g64 to d64 is enhanced to work on disks with a track offset, 
i. e. the content that should be on track 1 is actually on track (1+k), 
the content that should be on track 2 is actually on track (2+k) and so on. 


## Introduction

NIBTOOLS is a disk transfer program designed for copying original disks 
and converting into the G64 and D64 disk image formats. These disk images
may be used on C64 emulators like VICE or CCS64 [2,3] and can be transferred
back to real disks.

REQUIREMENTS:

- Commodore Disk Drive model 1541, 1541-II or 1571, modified to support
  the parallel XP1541 or XP1571 interface [1]

- XP1541 or XP1571 cable
   *AND*
- XE1541, XA1541, or XM1541 cable [1]

  *OR* 
- XEP1541, XAP1541, or XMP1541 combination cable [1]
  *OR* 
- XUM1541 (ZoomFloppy) with OpenCBM 0.5+
		
- Windows NT/2000/XP/Vista/Windows 7, x64 or x86 Editions, with OpenCBM 0.4.2 or higher
  Linux with OpenCBM 0.4.0 or higher,
  MS/DR/Caldera DOS and cwsdpmi.exe software (no longer tested but still compiles with DJGPP for old <=P3 hardware)
     
## Usage

Reading real disks into disk images:

1. connect 1541/71 drive to your PC's parallel port(s), using
the XE1541/XA1541 and the XP1541/71 cables, XEP/XAP/XMP combo cable, or ZoomFloppy.

2. insert disk into drive and start NIBTOOLS:
    nibread [options] filename.nib

3. use nibconv to convert between different formats:
    nibconv filename.nib filename.g64
    nibconv filename.nib filename.d64

    nibconv filename.nbz filename.g64
    nibconv filename.nbz filename.d64

    nibconv filename.d64 filename.g64
    nibconv filename.g64 filename.d64

Writing back disk images to a real disk:

1. connect 1541/71 drive to your PC's parallel port(s), using
the XE1541/XA1541 and the XP1541/71 cables, or XEP/XAP/XMP combo cable..

2. insert destination disk into drive and start NIBTOOLS:
    nibwrite filename.nib
    nibwrite filename.nbz
    nibwrite filename.g64
    nibwrite filename.d64


## References

The latest version of this program is available on https://c64preservation.rittwage.com/dp.php?pg=nibtools

[1] Circuit-diagrams and order form for the adaptor and cables
      http://sta.c64.org/cables.html  (diagrams and shop for X-cables)
      http://sta.c64.org/xe1541.html  (XE1541 cable)
      http://sta.c64.org/xa1541.html  (XA1541 cable)
      http://sta.c64.org/xp1541.html  (XP1541/71 cables)

[2] CCS64 homepage
      http://www.computerbrains.com/ccs64/

[3] VICE homepage
      https://vice-emu.sourceforge.io/


"Thank you!" to all people who helped me out with information and testing

* Andreas Boose         
* Joe Forster           
* Michael Klein        
* Matt Larsen          
* Mat Allen (Mayhem)
* Chris Link            
* Jerry Kurtz
* Wolfgang Moser       
* H�kan Sundell         
* Nicolas Welte         
* Tim Schurman
* Spiro Trikaliotis
* Nate Lawson
* Joerg Droege
* Quader
* Jani
* LordCrass

(C) 2004-16 Peter Rittwage, Markus Brenner, and friends
Based on routines from MNIB, which is (C) 2000-04 Markus Brenner.

