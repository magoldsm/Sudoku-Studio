FAQ

**Q. I want to transfer my Sudoku registration to a new PC, how do I do this?**

**A**. First, on your current PC, run SadMan Sudoku, show the About page (using the Help -> About menu option) and make a note of your registered name and key. Secondly, download the installation kit from the [download](http://www.sadmansoftware.com/sudoku/download.htm) page, and use this to install SadMan Sudoku on your new PC. Run it, then enter your registered name and key using the Help -> Enter Registration Key menu option. Note: the registered name is case-sensitive and must be re-entered exactly as it was shown.

**Q. My PC crashed and I've lost my registration details. How do I get them back?**

**A**. Send me an [email](http://www.sadmansoftware.com/contact.htm) with your name and enough of your address to enable me to identify your registration details, and I'll reply with your key.

**Q. Single-step and ten-step are disabled, how do I enable them?**

**A**. These functions are disabled whenever trial-and-error is an allowed solving technique. This is because, with  trial-and-error enabled, the grid could be left in an incorrect state after the allowed number of steps. To  enable the single and ten step functions, disable trial-and-error (Actions -> Solving Techniques -> Trial and  Error.)

**Q. I've entered a few pencil marks, and now Sudoku won't find the solution - why not?**

**A**. The built-in solver uses the same pencil marks as you do. This is so you can assist the solver if it gets  stuck, but it means the solver will never add pencil marks to a cell if there are some already present -  otherwise it could undo eliminations made because of your superior logic. The downside of this, is that if you  enter any pencil-marks into a cell, you need to enter all of them. The easiest way to do this is with the  "automatic-pencil-mark" button. If any cell contains incomplete (or incorrect) pencil marks, the solver may not  be able to find the solution.

**Q. I've used the OCR function to import a puzzle from an image, but some of the numbers are wrong - will you fix  it?**

**A**. The OCR will never be perfect for every puzzle. There are just too many variations in number font, puzzle style  and image quality. On the whole, I think it does a pretty good job for a poor-mans-OCR, and I'm unlikely to be  able to make it perfect for images from every possible source.

**Q. I've got an idea that I think will improve SadMan Sudoku - will you implement it?**

**A**. Perhaps. Email me with your idea, and I'll add it to the list for consideration. No promises though. (Be sure  to put "Sudoku" into the subject line of your email, so my spam filter doesn't delete it.)

**Q. Please will you implement my favourite variation?**

**A**. No, almost certainly not. I just don't have the time, nor the willpower, to work out the logic for any  variations. Sorry.

**Q. I've turned off the menu bar, how can I turn it back on?**

**A**. Right-click on any toolbar, including the effects or highlight toolbars and the status bars, and select the Menu option.

**Q. I thought Sudoku created a library of pre-generated puzzles, so why am I still waiting for a puzzle to be created?**

**A**. There are three possibilities:

Sudoku will use the idle time of your computer the create a library of puzzles, but it can only do that while it is running. If sudoku has not been running for very long, then the library may not yet contain many puzzles.
Perhaps you are looking for a puzzle that has very specific attributes, if so, it's possible the library does not contain a puzzle that meets your requirements. You could try broadening your requirements.
Have you disabled the background generation? Check the options page (View -> Options) and ensure the "Create puzzles in the background" option is checked. (On some older computers, creating puzzles in the background may adversely affect the response time for the user. If this is the case, disable background generation while you are actively using sudoku, but re-enable it periodically to allow the library to be topped up.)

**Q. I've just started using SadMan Sudoku, and noticed that the CPU usage is very high. Also, the cooling fan keeps coming on. Why is this?**

**A**. Sudoku is busy building up a library of puzzles. It does this in the background so that there's always a puzzle ready when you want one. Once the library is full, which may take several hours, it will stop.
You can:

  Leave it running unattended for a while to give it a chance to fill its library without the fan bothering you.
  Disable background creation of puzzles. View -> Options -> Options tab -> Create puzzles in the background. But if the library is empty when you want a new puzzle, you'll have to wait while one is generated.
  View some stats on the library to give you an idea how much longer it will be annoying you. View -> Library statistics. The library is set to hold 1000 puzzles, roughly split across the various grades, with a bit of leeway, works out at 138 puzzles per grade, but with fewer diabolical.

**Q. When I try to view the solution log in the internal viewer nothing happens, what's wrong?**

**A**. The internal viewer opens behind the main Sudoku window. If you run with the main window maximised, it's possible that you won't notice the log viewer behind it. You can either restore and resize the main window so that the log viewer is visible, or use Notepad to view the log. (Although one disadvantage of using Notepad is that it shows a static snapshot of the log, and is not updated as the puzzle is solved further.)

**Q. The button and menu option to view the solution log are disabled ("grayed out"). How can I enable them?**

**A**. The solution log only contains automatic moves, so if you haven't used an automatic solve method, it remains disabled.

Use the "Solve It", "Single Step", "Ten Steps", "Complete Naked Singles" or "Complete Hidden Singles" to put entries into the solution log and so enable the option.

**Q. Are SadMan Sudoku puzzles minimal, in that deleting any single clue will cause it to have multiple solutions?**

**A**. Not necessarily. Because SadMan Sudoku puzzles are symmetrical, it would be necessary to delete either two or four clues, to maintain the symmetry. Deleting any two or four clues would result in a puzzle with either multiple solutions, or that the in-built solver could not solve.

  **Q. What is the grading scheme, how are different grades assigned?**

  **A**. This is the grading scheme currently used, although this is likely to change in the future.
  
  Simple
  Naked Single only or Hidden Single only (depending upon a configuration option.)
  Easy
  (NumHiddenSingle > 0) or (NumNakedSingle > 0) (depending upon a configuration option.)
  Mild
  (NumNakedPairs + NumHiddenPairs + NumBlockBlockInteractions + NumBlockColumnRowInteractions) > 0
  Moderate
  (NumBlockBlockInteractions + NumBlockColumnRowInteractions > 0) and

      (NumNakedPairs + NumHiddenPairs > 0)
  Hard
  (NumXWing + NumXYWing + NumXYZWing + NumXYChain + NumForcingChains + NumColouring + NumNakedTriplets + NumHiddenTriplets) > 0
  Very Hard
  (NumXWing + NumXYWing + NumXYZWing + NumXYChain + NumForcingChains + NumColouring > 1) or

      (NumNakedTriplets + NumHiddenTriplets > 2)
  Fiendish
  (NumNakedQuads + NumHiddenQuads + NumSwordfish > 0) or

      (NumXWing + NumXYWing + NumXYZWing + NumXYChain + NumForcingChains + NumColouring > 2)
  Diabolical
  (NumTrial-and-Error > 0) or (NumNakedQuads + NumHiddenQuads + NumSwordfish > 1) or

      (NumXWing + NumXYWing + NumXYZWing + NumXYChain + NumForcingChains + NumColouring > 4)
  

  **Q. Iolo's System Mechanic Professional 7 and Avast's Spyware Blocker say SadMan Sudoku contains a trojan, what's going on?**

  **A**. Let me correct that. Iolo's System Mechanic Professional 7 and Avast's Spyware Blocker *mis*-identify SadMan Sudoku as containing a trojan.
  It appears that they are simply looking at the filename "Sudoku.exe", and since the adware "YazzleSudoku" also uses this, assume they are one and the same.
  They're not. SadMan Software Sudoku * is not* YazzleSudoku, and SadMan Software applications *do not* contain any adware, spyware, trojans, or any other form of malware.
  System Mechanic Professional 7 and Spyware Blocker are just plain wrong.
  (But don't just take my word for it, take a look at [McAfee's SiteAdvisor report](http://www.siteadvisor.com/sites/sadmansoftware.com/downloads/14803517/).
  This shows SadMan Sudoku to be completely safe, containing no malware and a "nuisance value" of zero.)
  

  

  You can safely add SadMan Software Sudoku to their "ignore" list, or alternatively, 
  you can stop the warnings by simply renaming the installed Sudoku.exe to something like SadManSudoku.exe,
  although you'll need to re-create any short-cuts on your start menu or desktop, and rename the help file
  to match.
  

  

  If you'd like further information on how to identify YazzleSudoku, take a look at the [CA entry](http://ca.com/us/securityadvisor/pest/pest.aspx?id=453097706).
  

  

  I contacted Iolo's support about this mis-identification on 26th June and again on 27 July 2008, but so far, their only response is to send me some spam for System Mechanic Professional 8. Not exactly helpful are they?
  

  

  I contacted Avast's suport on 24 July 2008, and they replied on the very next day. They say they will investigate the problem, and modify their signature database after they have confirmed the problem. Sounds promising, let's see how it goes.