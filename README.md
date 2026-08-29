<p align="center">
  <i>
    "What are you going to do when you're forgetful?"<br>
    "You're going to build a program that stops us?"<br>
    <b>— "Yes, I will."</b>
  </i>
</p>




# Prude

## Introduction
The inspiration behind **Prude** stemmed from a remarkably common yet frustrating scenario: stepping away from a workstation and forgetting to lock the session, only to return and find mischievous peers installing random utilities or navigating to questionable domains for a quick laugh. 

Rather than succumbing to the mundane standard of hitting `Win + L`, the philosophy behind Prude embraces a more proactive and entertaining approach: why not engineer a custom, lightweight sentry that permits completely normal day-to-day operation, yet instantly intercepts malicious or unauthorized activity with an unyielding security overlay? 

If an attacker or a nosy colleague attempts to hijack your session while you are distracted, they are instantly confronted with a blackout lockdown screen demanding credentials. While the Windows Task Manager can technically be triggered via hotkeys, it launches safely *behind* the topmost full-screen barrier, rendering it entirely inaccessible. Furthermore, shortcuts like `Win + R` are aggressively intercepted, neutralizing quick system manipulation. 

Positioned as a Proof of Concept (PoC), Prude serves as a foundational building block for custom parental control logic or simply as a definitive deterrent to keep unauthorized hands off your hardware.

## Technical Overview & Troubleshooting Journal
Developing a low-level background sentinel entirely in native C++ using the Win32 API presented several architectural hurdles that required deliberate engineering solutions:

* **The "Charles Dickens" False Positive Dilemma:** Initial iterations utilized a basic string search (`find()`) which resulted in humorous false alarms—such as searching for author *Charles Dickens* triggering a lockout because the substring `"dick"` was detected inside the name. This was successfully resolved by implementing a whole-word token boundary check (`containsWholeWord`), ensuring substrings embedded within legitimate compound words are ignored.
* **UI Freezing and "Not Responding" States:** When testing external termination methods (such as issuing a standard `taskkill` via command-line), the interception window initially locked up, entering Windows' dreaded "Not Responding" faded state and failing to accept keyboard input. This was mitigated by handling the `WM_CLOSE` message gracefully without blocking the application thread, ensuring the password input field remains responsive and interactive under pressure.
* **Global Low-Level Hook Reliability:** Early attempts to manage modifier keys via asynchronous state polling (`GetAsyncKeyState`) inside low-level keyboard hooks (`WH_KEYBOARD_LL`) led to race conditions and occasional dropped key combinations. The system was restructured to deterministically track modifier states (`Alt`, `Ctrl`, `Win`, `Shift`) directly from incoming message payloads, guaranteeing bulletproof shortcut blocking.
* **Intentionally Omitted Persistence:** Prude deliberately lacks any form of automated persistence or registry-based autostart mechanisms. It is intentionally designed as an ad-hoc companion helper—launched manually when you realize you are stepping away and wish to secure your environment temporarily. 

*(Historical footnote: Early developmental variants famously combined a strict numeric-only input style (`ES_NUMBER`) with a text password like "Adam", leading to amusingly locked-out troubleshooting sessions. Modern iterations feature standard text boxes coupled with `ES_PASSWORD`, masking input securely behind asterisks so wandering eyes remain completely clueless.)*

## Compilation
To compile the project cleanly using GNU C++ (MinGW) on Windows, execute the following command in your terminal:

```bash
g++ helper.cpp -o helper.exe -mwindows # you dont have to: (-lpsapi)
```
<div style="border: 3px solid red; padding: 15px; background-color: #ffe6e6; color: #990000; font-weight: bold; text-align: center;">
WARNING: DO NOT RUN THIS SOFTWARE ON ANY SYSTEM WITHOUT EXPLICIT PERMISSION AND AUTHORIZATION. UNAUTHORIZED USE, DEPLOYMENT, OR INTERCEPTION OF SYSTEM CONTROLS MAY VIOLATE APPLICABLE LAWS AND SECURITY POLICIES.
</div>
<blockquote style="border-left: 4px solid #ff4d4d; padding-left: 15px; color: #d6d6d6; background-color: #1a1a1a; padding: 15px; border-radius: 4px; font-family: sans-serif; margin: 20px 0;">
    <p style="margin: 0; font-style: italic;">&ldquo;Yes, I used AI to generate these keywords&mdash;unfortunately (or fortunately!), I don't have the memory or the 'experience' to list this many terms off the top of my head.&rdquo;</p>
    <p style="margin: 10px 0 0 0; font-weight: bold; color: #ff6b6b; font-style: normal;">&mdash; a Ty byś potrafił?</p>
</blockquote>
