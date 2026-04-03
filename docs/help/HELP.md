# Sudoku Studio Help

Welcome to Sudoku Studio! This help guide covers solving techniques and frequently asked questions.

## Table of Contents

- [Frequently Asked Questions](#frequently-asked-questions)
- [Glossary](#glossary)
- [Solving Techniques](#solving-techniques)
  - [Naked Single](#naked-single--singleton--sole-candidate)
  - [Hidden Single](#hidden-single--unique-candidate)
  - [Block/Column/Row Interactions](#blockcolumnrow-interactions)
  - [Block/Block Interactions](#blockblock-interactions)
  - [Naked Subsets](#naked-pair-triplet-quad--naked-subset--disjoint-subset)
  - [Hidden Subsets](#hidden-pair-triplet-quad--hidden-subset--unique-subset)
  - [X-Wing and Swordfish](#x-wing-and-swordfish)
  - [XY-Wing](#xy-wing)
  - [XYZ-Wing](#xyz-wing)
  - [XY-Chain](#xy-chain)
  - [Colouring](#colouring)
  - [Remote Pairs](#remote-pairs)
  - [Forcing Chains](#forcing-chains)
  - [Trial and Error](#trial-and-error)

---

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

---

glossary

# Glossary of Sudoku Terms and Definitions

The sudoku grid consists of nine horizontal rows, nine vertical columns, and nine 3 x 3 blocks (also called boxes). Rows are numbered 1 to 9, top to bottom. Columns are numbered 1 to 9, left to right. Blocks are numbered 1 to 9 top to bottom and left to right.
Rows, columns and block are all kinds of unit, and there are a total of twenty-seven units in a grid.
Every row, column and block contains nine cells, and every row, column and block must contain the digits 1 through 9.
Cells that share a column, row or block are known as "buddies".
The numbers given in the grid are the puzzle's clues.
The numbers you add, as a player, are "big numbers".
The numbers which could possibly go into an empty cell according to the rules of the game, are known as the empty cell's candidates, also known as pencil-marks.

---

Naked Single

## Naked Single / Singleton / Sole Candidate
    It is often the case that a cell can only possibly take a single value, when the contents of the other cells in the same row, column and block are considered. This is when, between them, the row, column and block use eight different digits, leaving only a single digit available for the cell.

    For example, in the partial puzzle below, the marked cell can only be a 6. All other digits are excluded by the contents of the
    other cells in the row, column and block.

---

Hidden Single

## Hidden Single / Unique Candidate
If a cell is the only one in a row, column or block that can take a particular value, then it must have that value.
This is because all rows, columns and blocks, must contain each of the digits 1 to 9.
For example, in the partial puzzle below, the marked cell is the only one in
block five that *can* hold a 2, and so it *must* hold a 2.

---

Block and Column / Row Interactions

## Block and Column / Row Interactions
Sometimes, when you examine a block, you can determine that a certain number must be in a specific row or column, even though you cannot determine exactly which cell in that row or column. This is enough information to remove that number from the candidate list for other cells in the same row or column, but outside the block.

For example, in the partial puzzle below, the 7 in block one can only occur in
column two. This means we can eliminate 7 from the candidates for the marked
cells.

---

Block / Block Interactions

## Block / Block Interactions
Firstly, If a number appears as candidates for only two cells in two
different blocks, but both cells are in the same column or row, it is possible
to remove that number as a candidate for other cells in that column or row.
For example, in the partial puzzle below, the green cells marked with the
asterisk are the only cells in blocks two and five that can contain a 3. This
means the 3 in column four must be in block two or five, as must the 3 in column
five. As there can be no other 3s in columns four or five, 3 can be eliminated
as a candidate for the cells in these columns for block eight.

  Secondly, in the example below, the cells marked with * are the only cells
  in blocks four and six that can contain a 2. This means that 2 can be
  eliminated from the candidates for the marked cells in block 5.

---

Naked Subset

## Naked Pair, Triplet, Quad / Naked Subset / Disjoint Subset
This technique is known as "naked pair" if two candidates are
involved, "naked triplet" if three, or "naked quad" if four.
If two cells in the same row, column or block have only the same two
candidates, then those candidates can be removed from the candidates of the
other cells in that row, column or block. This is because one of the cells must
hold one of the candidates, and the other cell must hold the other candidate -
so neither can go in any of the other cells.
This technique can be applied to more than two cells at once, but in all cases,
the number of cells must be the same as the number of different candidates. Each
cell doesn't need to have every member of the subset as candidates, but no cell
can have any candidates outside the subset.
For example, consider a row that has the candidates:

  {1, 7}, {6, 7, 9}, {1, 6, 7, 9}, {1, 7}, {1, 4, 7, 6}, {2, 3, 6, 7}, {3, 4,
  6, 8, 9}, {2, 3, 4, 6, 8}, {5}

(The single {5} indicates that this cell already holds the value 5.) You can
see that there are two cells that only have the same two candidates 1 and 7. One
of these cells must hold the 1, and the other cell must hold the 7, although we
don't know which is which. So 1 and 7 can be removed from the candidates for the
other cells. This reduces the candidates to:

  {1, 7}, {6, 9}, {6, 9}, {1, 7}, {4, 6}, {2, 3, 6}, {3, 4, 6, 8, 9}, {2, 3,
  4, 6, 8}, {5}

So now there are two cells that have 6 and 9 as the only candidates.
Repeating the process for these numbers leaves:

  {1, 7}, {6, 9}, {6, 9}, {1, 7}, {4}, {2, 3}, {3, 4, 8}, {2, 3, 4, 8}, {5}

Now we have a cell with a single candidate - i.e. we have reduced the
candidates to the extent that we have determined the only value that can
possibly go into this cell.
In the puzzle below, the green cells contain the naked pair 2 and 3, allowing
2 and 3 to be eliminated from the candidates for the other cells in column nine.

---

Hidden Subset

## Hidden Pair, Triplet, Quad / Hidden Subset / Unique Subset
This technique is known as "hidden pair" if two candidates are
involved, "hidden triplet" if three, or "hidden quad" if
four.
This technique is very similar to naked subsets, but instead of affecting
other cells with the same row, column or block, candidates are eliminated from
the cells that hold the subset. If there are N cells, with N candidates between
them that don't appear elsewhere in the same row, column or block, then any
other candidates for those cells can be eliminated.
For example, consider a block that has the following candidates:

  {4, 5, 6, 9}, {4, 9}, {5, 6, 9}, {2, 4}, {**1**, 2, **3**, 4, **7**},
  {**1**, 2, **3**, **7**}, {2, 5, 6}, {**1**, 2, **7**}, {8}

(The single {8} indicates that this cell already holds the value 8.) You can
see that there are only three cells that have any of the candidates 1, 3 or 7.
(These cells have other candidates too, but they're the ones that we can
eliminate.) Three candidates with only three possible cells between them, leads
to the conclusion that one of the candidates must be in each of the cells,
although we can't say which is which. So, obviously, these three cells cannot
hold any other value, meaning we can eliminate any other candidates for these
cells.
After making the elimination in this example, we're left with:

  {4, 5, 6, 9}, {4, 9}, {5, 6, 9}, {2, 4}, {1, 3, 7}, {1, 3, 7}, {2, 5, 6},
  {1, 7}, {8}

I get many emails pointing out that one of the cells doesn't have 3 as a
candidate, but this makes no difference at all. The important point is that
there are only three cells in which three candidates appear, even if they're not
all in each.
The second question I get asked, is why not make the subset 1, 2 and 7. The
answer is because there are five cells containing any of these numbers, and
three candidates over five cells doesn't allow any eliminations at all.
In the puzzle below, the green cells have the hidden pair 3 and 5.

Naked subsets and hidden subsets are related - I usually describe them as
being opposite sides of the same coin. If a naked subset is present, then so is
a hidden one, although it may be longer and so harder to spot. The opposite is
also true, if a hidden subset is present, so is a naked one. They obey the
following relationship:

  NumberOfDigitsInNakedSubset + NumberOfDigitsInHiddenSubset +
  NumberOfFilledCellsInUnit = 9

or to put it another way:

  NumberOfDigitsInNakedSubset + NumberOfDigitsInHiddenSubset =
  NumberOfEmptyCellsInUnit

---

X-Wing and Swordfish

## X-Wing
  In the puzzle below, the only cells in rows one and eight that can contain
  a 9 are those coloured green.  Since there must be a 9 in both row one
  and row eight, but they cannot occupy the same column, it follows that either
  the top-left and bottom-right marked cells contain the 9s, or the bottom-left
  and top-right cells do. (It can't be the bottom-right and top-right, nor the
  bottom-left and top-left, as then there would be two 9s in the same column.
  Similarly, it can't be top-left and top-right, nor bottom-left and
  bottom-right as then there would be two 9s in the same row.) So, we can't say
  whether the 9s are in top-left and bottom-right, or bottom-left and top-right,
  but either way, it excludes 9s from the other cells in both columns. The end
  result is that 9 can be eliminated from the candidates for other cells in both
  of the affected columns (coloured blue in this example.)

---

X-Wing and Swordfish

##  Swordfish
Look for three columns with only two candidates for a given digit. If these
fall on exactly three common rows, and each of those rows has at least two
candidate cells, then all three rows can be cleared of that digit - except in
the defining cells. This is the original, "restrictive" definition. It
has since been realised that a more relaxed definition is possible, in that the
three columns can each have two or three candidates for the given digit - as
long as they fall on the three common rows.
Rows and columns can be swapped in the above description of course.

Consider the following partially complete puzzle:

At this point, after performing candidate reduction using other techniques,
we have a swordfish in the 1s. The marked cells are the only cells in columns
one, four and six that can contain a 1, and because each of those columns must
only contain one 1, and since the cells also share three common rows, the cells
are linked in a similar manner to the X-wings. The net effect is that we can
eliminate 1 from the candidates of the other cells in rows two, six and nine.

---

XY-Wing

## XY-Wing
This is similar to a short [forcing chain](forcingchain.htm) consisting of two links for each candidate, but instead of placing a number, it allows for candidate elimination.
This a very common pattern in the harder puzzles.
In the partial puzzle below, consider the cells that have only the candidates
shown:

  
    
      
        
          
             
             
             
          
          
             
            XY
             
          
          
             
             
             
          
        
      
      
        
          
             
             
             
          
          
             
            XZ
             
          
          
             
             
             
          
        
      
    
    
      
        
          
             
             
             
          
          
             
            YZ
             
          
          
             
             
             
          
        
      
      
        
          
             
             
             
          
          
             
            *
             
          
          
             
             
             
          
        
      
    
  

  It can be easily seen that whichever value is in XY, the cell marked with
  the asterisk cannot be Z.
  
    	if XY = X, then XZ = Z, so * cannot be Z

	if XY = Y, then YZ = Z, so * cannot be Z
  
  This allows Z to be eliminated from the candidates for the marked cell.

  The cells don't need to form a perfect rectangle, but XY and XZ, and XY and YZ need to be linked by being in the same unit (that is the same column, row or block.) Once you've got this arrangement, you can eliminate Z from the candidates of all cells that occupy the intersection of the units containing XZ and
  YZ.
   
  Other possible combinations:

  
  
    
      
        
          
             
            XY
             
          
          
             
             
             
          
          
            YZ
             
             
          
        
      
      
        
          
             
            XZ
             
          
          
             
             
             
          
          
            *
            *
            *
          
        
      
    
  
    

   
    
      
        
          
            *
            XY
            *
          
          
             
             
             
          
          
            YZ
             
             
          
        
      
      
        
          
             
            XZ
             
          
          
             
             
             
          
          
             
             
             
          
        
      
    
  

    

   The astute among you will notice both the above examples have XY, XZ and
   YZ in the same relative locations, and so can be combined to give:
  
   
    
      
        
          
            *
            XY
            *
          
          
             
             
             
          
          
            YZ
             
             
          
        
      
      
        
          
             
            XZ
             
          
          
             
             
             
          
          
            *
            *
            *
          
        
      
    
  

  All the cells marked with an asterisk can have Z removed from their
  candidates.
In the puzzle below, the XY-wing in the green cells allows 7 to be eliminated
from the blue ones.

---

XYZ-Wing

## XYZ-Wing
This is a variation of an [XY-wing](xywing.htm). In the partial puzzle below, consider the cells that have only the candidates shown. Any cells that share a unit with all three cells XYZ, XY and YZ, can have Z eliminated from their candidates.

  
    
      
        
              
                 
                YZ
                 
              
              
                XYZ
                *
                *
              
              
                 
                 
                 
              
            
          
      
        
              
                 
                 
                 
              
              
                 
                XZ
                 
              
              
                 
                 
                 
              
            
          
        
      
    

    

    It can be easily seen that which ever value is in XYZ, the cells marked with the asterisk cannot be Z.
    
      if XYZ = X, then XZ = Z, so * cannot be Z

      if XYZ = Y, then YZ = Z, so * cannot be Z

      if XYZ = Z, then * cannot be Z
    
    This allows Z to be eliminated from the candidates for the marked cells.
In the puzzle below, the XYZ-wing in the green cells allows 1 to be
eliminated from the blue cell.

---

XY-Chains

## XY-Chains
This is a technique that allows you to make eliminations by following a chain of cells that have only two candidates each.

Consider the following puzzle fragment:

  
    
      
        
              
                {1,3}
                 
                 
              
              
                 
                 
                 
              
              
                *
                *
                *
              
            
          
      
        
              
                 
                {3,7}
                 
              
              
                 
                 
                {6,7}
              
              
                 
                 
                 
              
            
          
      
        
              
                *
                *
                *
              
              
                 
                {6,8}
                 
              
              
                 
                 
                {1,8}
              
            
          
        
      
      
       

      (The numbers in curly brackets { } are the only candidates for the cell.)

      We can build up an implication chain thus:

      
        if r1c1 = 3 then r1c5 = 7, so r2c6 = 6, so r2c8 = 8, so r3c9 = 1

      
      So we know that either r1c1 is 1, or if r1c1 is 3, then r3c9 must be 1. So any cells that share a unit with both of these cells can have 1 eliminated. These are the cells marked by the asterisks in the above example.

      More generally, as long as the same number is "unused" at both ends of the chain, then this technique allows the elimination of that number for any cells that share a unit with both end cells. 

     

Consider the following puzzle:

We can build up the implication chain:

  if r1c2 = 5 then r6c2 = 4, and so r6c8 = 7, and so r6c1 = 1.

So we know that either r1c2 is 1, or if r1c2 is 5, then r6c1 must be 1. So
any cells that share a unit with both of these cells can have 1 eliminated.
These are the blue cells in the above example.
More generally, as long as the same number is "unused" at both ends
of the chain, then this technique allows the elimination of that number for any
cells that share a unit with both end cells.

---

Colouring

## Colouring
Colouring is a technique similar to forcing chains in that it looks for
chains of connected cells. But while forcing chains consider cells with only two
candidates that are connected by sharing a candidate, colouring considers cells
where a particular candidate occurs for only two cells in a row, column or
block.
Consider this example:

This example contains two separate conjugate chains of 1s - marked by orange/green
and pink/blue. We're
only interested in part of the orange/green chain, as colouring can show that
r8c4 cannot be a 1, by considering the chain r8c3 => r4c3 => r5c1 =>
r5c4. The logic goes like this:

  if r8c3 is 1 then

      r8c4 cannot be 1 as both cells occupy the same row.

  

  if r8c3 is not 1 then

      r4c3 must be 1 (because column three must contain a 1
  somewhere), so

      r5c1 cannot be 1, as it is in the same block, so

      r5c4 must be 1 (because row five must contain a 1
  somewhere), so

      r8c4 cannot be 1.

So, in either case, we've shown that r8c4 cannot be 1, we've eliminated a
candidate.

The conjugate chain for the above example is r8c3 => r4c3 => r5c1 =>
r5c4. Since this is a chain of *conjugate* cells, the links have
alternate truth states - indicated by the green and orange shading in the above
example, either the green cells must be true, that is, hold a 1, or the orange
cells, but not both. Any other cells in the grid that share a unit with both a
link having a true state, and a link having a false state, cannot hold the
candidate and that candidate can be eliminated. This is how 1 was eliminated for r8c4
above.

Here's another example:

In this example, we can show that the blue cells cannot hold the 8. This is
because if r9c5 holds 8, then, following the conjugate chain, so must r8c4, and
since both cells are in the same block, they can't both hold 8. This means that
none of the blue cells can hold 8, and so all the pink cells must. Note: r4c4
and r8c4, and r9c5 and r9c8 are not conjugates, and so cannot be used to colour
each other, because of the candidate 8 in r9c4, 
 

When a unit has only two cells with a particular candidate, those cells are
"conjugates" of each other, and are said to be "strongly
linked". From the rule of Sudoku, we know that one of these cells must hold
the number, and other cannot. Conversely, if we know that one of the cells
cannot hold the number, then the other must. This allows us to form chains of
cells, with successive cells having alternate "colours". (The term
"colouring" is used because the technique is analogous to marking up
the grid using coloured pens.) We don't know which colour represents the true
state, but examination of the chain may enable us to make deductions leading to
the elimination of candidates.
 

**Multi-Colouring**: It is sometimes possible to use seperate colouring chains to make eliminations. There are two commonly used methods.

**Method 1**. This technique can make eliminations because one of the chains indicates which of the states is the false state for the other chain. Consider the following puzzle:

[Image: ../images/sudokucolouring4.png]

This example contains two separate conjugate chains of 4s - marked by orange/green and pink/blue.
Blue cells is weakly linked to both an orange and green cells of the
"other" chain, and this is the key pattern to watch for. They're not conjugates, or strongly linked, as there are other cells with a candidate
4 in the same column or row. (Weakly linked cells means that one being true will cause the other to be false, but one being false does *not* cause the other to be true.)
We can use this to show that both the blue cells can have the candidate 4 eliminated. Remember the premise of colouring; either all pink *or* all blue cells must be true, and for the other chain, either all orange *or* all green cells must be true. So if the
orange cells are true, then the blue at r8c2 must be false because of their weak
link. Likewise, if the orange cells are false, and the green cells are true,
the blue at r9c9 must then be false. So either way, the blue cells must be
false.

 

**Method 2**. This technique can be used to connect together apparently separate chains. Consider the following puzzle:

[Image: ../images/sudokucolouring3.png]

This example contains two separate conjugate chains of 4s - marked by orange/green and pink/blue.
These two chains can be joined together to make one longer chain. Cells r8c2 and r8c9 are weakly linked. They're not conjugates, or strongly linked, as there is another cell with a candidate 4 in the same row. Weakly linked cells means that one being true will cause the other to be false, but one being false does *not* cause the other to be true. The
two chains both have two cells that are weakly linked to cells in the other chain, r8c2 and r8c9, and also r5c2 and r5c8. What's more, these cells are linked to opposite links in the true/false alternate states.
So, if r5c2 is true, then r5c8 is false, and then, because of the orange/green conjugate chain, r8c9 is true. Alternatively, if r5c2 is false, then
because of the pink/blue conjugate chains, r7c7 is true, and so r8c9 is false, and then, because of the
orange/green conjugate chain, r5c8 is true. In other words, the two weak links have now joined the two separate chains into one strongly linked one. The net effect in the above example is that the candidate 4s in r5c3 and r8c7 can both be eliminated as they both share units with strongly conjugated cells.
 

This technique is known as "simple colouring", and
"multi-colouring" when apparently separate colouring chains can be
joined. There are also other colouring techniques, for example
"super-colouring", a technique that makes deductions by combining the
implications from conjugates of all candidates for all cells, although this
technique is beyond the ability of most, if not all, human solvers.

---

XYZ-Wing

## Remote Pairs
This technique is a combination of [naked pairs](nakedsubset.htm)
and [colouring](colouring.htm), and is a special case of [XY-chains](xychain.htm).
It's often easier for a human to spot than an XY chain.
Consider the following puzzle:

Consider the chain of cells r2c6, r2c8, r3c7 and r4c7. We notice that

  if r2c6 is 1 then r2c8 is 2, so r3c7 is 1, so r4c7 is 2.

and

  if r2c6 is 2 then r2c8 is 1, so r3c7 is 2, so r4c7 is 1.

So, in a similar manner to simple colouring and XY chains, we can be sure
that any cells that share units with both r2c6 and r4c7 cannot be 1 or 2. So in
this example, we can eliminate 1 and 2 from the candidates for r4c6.
**Note**: the chain must contain an even number of cells or else
elimination is not possible. For example, consider the chain r2c8, r3c7 and
r4c7. You might think that these would allow elimination of 1 and 2 in r5c8, but
this is not the case. If r2c8 is 1, so is r4c7, and if r2c8 is 2, so is r4c7.
All we know is that both cells contain the same number, but not which one it is.
We cannot make any elimination to r5c8.

---

Forcing Chains

## Forcing Chains
Forcing chains is a technique that allows you to deduce with certainty the
content of a cell from considering the implications resulting from the placement
of each of another cell's
candidates.

Consider r2c1. This has the two candidates, 1 and 2. We will consider the
implications of each of these candidates in turn.

  if r2c1 = 2, then r1c2 = 7

  if r2c1 = 1, then r5c1 = 2, and so r6c2 = 1, and so r6c8 = 3, and so r1c8 =
  2, and so r1c2 = 7 

So whichever of the two possible values are placed into r2c1, we've deduced
that r1c2 *must* hold a 7. In other words, whichever chain of cells we
follow, a certain cell is forced to have a specific value.
	**Note**: unless the puzzle has multiple solutions, one of the
	considered candidates *must* be incorrect. This means it *must*
	eventually lead to either a contradiction or a dead end. If, when
	considering a single candidate, you reach a dead end, or find two chains
	that lead to different conclusions, you can eliminate that candidate from
	the starting cell. This is verging onto trial-and-error, and SadMan Software
	Sudoku doesn't do this as part of the forcing chain strategy. However, it
	can be useful when solving manually.

---

Trial and Error

## Trial and Error
There are some that would argue trial and error is not a logical technique,
and is no better than guessing. Although it's not a technique I like to use, I
do consider it logical. When further moves seem impossible, trial and error may
be the only way forward. Indeed, some puzzles cannot be completed without
it.
The technique involves selecting one candidate for a cell - without any
particular reason for that selection - and then seeing whether the puzzle can
then be completed. If it can, well done (although, there could also be other
solutions - test the other candidates too.) If not, the trial and error move, and
any subsequent moves, are undone, and a different choice is made. For some
puzzles, it may be necessary to use trial and error several times. For others,
it may be required only once.
In order to better manage the complexity, it's usual, if possible, to choose
a cell with only two candidates, but that doesn't have to be the case.
It's worth noting, that this technique alone will always generate a solution
if the puzzle can be solved, no other technique can guarantee that. But when
used alone, it becomes the equivalent of a brute-force attack

---

