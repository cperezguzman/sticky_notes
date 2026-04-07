Building a personal sticky notes-like app for ubuntu

sticky notes --> SN
cut off by another thought --> XXXXX

in-scope:
* want it to be able to stay on screen no matter what other windows I click on
  (only disappears when I close or minimize it)
* can shrink or expand the window
* can use mouse and keys to move curser
* saves even after closing but only within the app 
  (? idrk if thats possible w/o file-saving)
* text wrap-around


out-scope:
* no undo/redo
* no saving into a file
* no non-roman characters

Thoughts while sketching:
* I need a container to hold and sort through the different SNs created.
  Should I create a class or smth to hold it ? Or maybe the app itself is
  the container?
* I should have just one function for determining how long ago the last edit was 
  I was thinking of having different functions depending on how long it has been 
  since the last edit (e.g., convert to time if it only has been hours, days 
  if > 24 hours passed, etc.) but that seems like too much, so instead i'll do an internal
  check of whether `now - previous_edit_time` > 24 hours, > 31 days, etc. XXXXX

  Actually, I'll have a choice that lets users decide how they want to display their
  "last edited": _ days, _ hours, _ seconds ago OR any combination of these (including
  just one, e.g., 74 hours ago or 266400 seconds ago). I'll pass that choice through 
  an extra argument in `get_last_edit` 


Sketches:
1) 
is the each tab of the SN just a struct ?

the struct can have:
* a vector of strings to contain the text in the SN
* an integer representing the id of that specific SN 
* another integer for a timestamp representing  when it was last edited
* another integer for the date that it was created
* a string for the title of the SN
* 

smth like:

```cpp
struct sticky_note {
    std::vector<std::string> text;
    int id;
    int last_edited;
    int created;
    std::string title;
}
```

2)
add functions that manipulate the struct

functions can be:
* converts the time extracted for last_edited and created from the 
  `std::chrono::time_point<std::chrono::system_clock>` type into readible date and time
* updates the last_edited time
* something that creates/modifies title of SN
* can insert/delete from the vector of strings (modifying the notes as the user does it)
* 
