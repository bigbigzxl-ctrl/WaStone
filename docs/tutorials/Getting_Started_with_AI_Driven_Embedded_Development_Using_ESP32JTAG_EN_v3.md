# Tutorial: Getting Started with AI-Driven Embedded Development Using ESP32JTAG

> A hands-on tutorial for ESP32JTAG users, showing how AI, AI Embedded Lab, and ESP32JTAG can generate, build, flash, verify, and iterate on embedded firmware for an STM32 Blue Pill board.

## Introduction

This tutorial demonstrates a workflow that is fundamentally different from traditional embedded development.

In the past, even a simple LED blink program usually meant doing everything manually: writing code, editing it, building it, flashing it, observing the result, and then debugging and fixing issues by hand. The entire process was essentially a human directly driving tools.

Now, with ESP32JTAG, AI Embedded Lab, and an AI tool such as Codex or CloudCode, that workflow can be handed over to AI. The AI can generate code from natural-language instructions, build it, flash the target, verify the outcome, and continue repairing problems until the task is complete.

More importantly, when combined with the Data Capture capability of ESP32JTAG, even the final step of visually checking whether an LED is blinking can be moved from a human observer to machine confirmation. That is what makes this a real AI-driven closed loop.

In this tutorial, **AI-driven** means that the AI is not merely helping with code writing. It is responsible for the full execution loop: **generate → build → flash → verify → repair → verify again**.

---


## What “AI-Driven” Means Here

In this tutorial, **AI-driven** does not mean AI merely acts as a coding assistant that fills in a few lines, answers questions, or offers suggestions.

Here, **AI-driven** means the AI is placed in the execution position. It does not just participate. It drives a full development loop:

- generate code from a natural-language goal,
- build and flash the target,
- run the program,
- observe and verify the outcome,
- diagnose failures,
- repair the implementation and iterate,
- and continue until the task is complete.

In other words, the AI is no longer just an assistant. It is taking over a large part of the execution work that engineers previously had to perform by hand.

---

## Traditional Flow vs. AI-Driven Flow

The difference becomes clearer when the two workflows are placed side by side.

### Traditional embedded workflow

- The engineer sets up the project.
- The engineer writes the code.
- The engineer builds and flashes manually.
- The engineer observes behavior manually.
- The engineer reads documentation, diagnoses issues, and repairs bugs.
- The engineer repeats the loop until the task is complete.

### AI-driven embedded workflow

- The human provides the goal, constraints, and acceptance criteria.
- The AI generates the code.
- The AI builds and flashes the program.
- The AI verifies the result.
- The AI consults documentation, analyzes failures, and repairs the implementation.
- The AI keeps iterating until the task is complete or a subproblem is temporarily set aside.

The real shift is not simply that “AI writes code faster.” The real shift is that the execution loop itself starts moving from “humans directly driving tools” to “humans driving AI, and AI driving the tools and the workflow.”

---

## What You Need

Before getting started, prepare the following:

- An **ESP32JTAG**
- An **STM32F103C6T6 Blue Pill** board
- 3 jumper wires for SWD
- 1 optional jumper wire for machine confirmation, to route a target signal into an ESP32JTAG capture pin
- A **Codex Pro** or **CloudCode Pro** account
- **AI Embedded Lab** downloaded from GitHub

For getting started, a standard Codex Pro or CloudCode Pro account is typically enough. Once the AI tool and AI Embedded Lab are ready, and the hardware is connected, the workflow can begin.

---

## Hardware Setup

The first step is to connect ESP32JTAG to the STM32 Blue Pill through SWD.

The wiring is very simple and uses only three wires:

- **GND → GND**
- **SWCLK → SWCLK**
- **SWDIO → SWDIO**

In this setup, the debugging connection on ESP32JTAG comes from **P3**, also referred to as **Port C**. The two signal wires in the middle connect to the SWD clock and SWD data pins on the STM32 board.

Once these three wires are connected, the target board is ready for AI-driven bring-up.

[stm32f103c6t6_esp32jtag_hw.jpg](images/stm32f103c6t6_esp32jtag_hw.jpg)

---

## Software Setup

The software side is also straightforward.

First, you need an AI tool. You can use **Codex** or **CloudCode**. Both are strong options and both work well for this kind of task. You can subscribe through their official websites.

Once the account is ready, download **AI Embedded Lab**. It is available from GitHub, distributed under the Apache License, and provided as open-source software.

With both the AI tool and AI Embedded Lab in place, you can start issuing tasks in natural language instead of manually writing the initial project code yourself.

[stm32f103c6t6_esp32jtag_sw.jpg](images/stm32f103c6t6_esp32jtag_sw.jpg)

---


## Why ESP32JTAG Matters in This Workflow

In this tutorial, ESP32JTAG is not just a generic programmer or debugger. It matters because it provides the hardware-side capabilities that make a real AI-driven loop possible.

It is important in at least several ways:

- **It can flash and control the target through SWD**, covering the most basic build-and-program path.
- **It can perform Data Capture**, so the system can observe electrical signals directly instead of relying only on a human watching an LED or a terminal window.
- **It can receive and display UART data**, making target-side communication visible through a browser interface.
- **It enables machine confirmation**, which means the workflow can move from “a human thinks the program ran” to “the machine confirmed the program ran from the signal level.”

That is a meaningful difference.

Many tools can flash firmware. Many tools can debug firmware. But not every tool can combine **flashing, interaction, capture, result readback, and machine-level verification** in one workflow.

Once those capabilities are brought together, AI is no longer just writing code. It can drive a much more complete embedded validation loop.

---

## First Task: Make the LED Blink

LED blink has always been the embedded-system equivalent of Hello World. We have all done it before, but this time the process is different: the AI does the work.

You simply tell the AI something like this:

> I have an STM32F103C6T6 Blue Pill board connected through SWD to ESP32JTAG. Please create an LED blink program, generate the code, build it, flash it, and verify that it runs.

For the developer, this is a completely different experience. In the traditional flow, you would set up the project, write the initialization code, configure the clock and GPIO, build, flash, and debug. Here, the only requirement is to describe the goal clearly.

Once the program is generated, built, and flashed successfully, the onboard LED begins blinking.

That result matters because it shows that AI is not merely “helping with some code.” It is executing an actual embedded bring-up workflow on your behalf.

---

## A Simple Change: Double the Blink Rate

The next step is to give the AI an even simpler natural-language instruction:

> I can already see the LED blinking. Good. Now please modify the program so that it blinks twice as fast.

The AI then updates the program, rebuilds it, reflashes the board, and runs the new firmware. Soon after, the LED is blinking at twice the previous rate.

This example is simple, but it illustrates something important.

In a traditional workflow, this would still mean editing code manually, rebuilding, reflashing, and checking the result. In an AI-driven workflow, the entire change can be triggered by a single sentence.

That is the point where the development process starts to feel fundamentally different.

---

## From Human Confirmation to Machine Confirmation

At this stage, development efficiency is already much higher. But if the final step still depends on a human looking at the LED and deciding whether it is blinking, the loop is not yet complete.

This is where the **Data Capture** capability of ESP32JTAG becomes important.

An additional wire can be used to connect **PC13** on the target board to **ESP32JTAG P0.0** (also written as **PA0**, meaning the same thing). Then the AI can be instructed as follows:

> Now I have connected PC13 to ESP32JTAG P0.0 (or PA0, same meaning). Please machine-confirm the visible LED.

For human eyes, a low-frequency blink is easy to see. For machine capture, however, that visible blink may be too slow for convenient and reliable signal acquisition. So the AI may temporarily switch to a higher-frequency electrical probe—perhaps a few kilohertz, tens of kilohertz, or more—to make capture easier and verification more robust.

After the machine check, the result showed that:

- the PC13-to-P0.0 path was machine-confirmed,
- a large number of edges were observed during the capture window,
- the high and low counts were balanced,
- the MCU was indeed driving the PC13 net, and
- ESP32JTAG was indeed observing that same signal at P0.0.

If you are curious, you can capture the and check the waveform using the ESP32JTAG web interface.

[gpio_toggle_image.jpg](images/gpio_toggle_image.jpg)

Why does this matter?

Because the system is no longer depending on a person to say, “Yes, I can see it blinking.” Instead, the machine has electrically confirmed that the signal is active and behaving as expected.

This is a key step. It turns AI-driven embedded development from automatic code generation into a genuine closed-loop verification workflow.

There is one important caveat: if the verification uses a temporary high-frequency test signal, then it proves strong electrical machine confirmation, not necessarily that a human could visually perceive the LED blinking at that temporary frequency. But for automated verification, that machine-level confirmation is actually the stronger result.

---

## Going Further: A UART Communication Example

LED blink is only a beginning.

ESP32JTAG also has UART input capability. That means it can receive UART output from the target board and present that data through a browser-based interface.

So instead of verifying only an LED, the target UART can also be connected to the ESP32JTAG UART, allowing AI to create and validate a real communication example.

I then gave the AI a new task: design a communication program that can emit UART data from the target and make that data observable through ESP32JTAG.

It actually did it.

The AI designed the program and wrote the code. The resulting implementation was only about 258 lines long, yet it was already enough to serve as a practical validation program. That is remarkable, because this was not a prebuilt demo being reused. It was a new program designed and implemented directly from the natural-language goal.

This shows that AI is no longer just filling in boilerplate from templates. It is beginning to design, implement, and validate toward a stated objective.

---

## From a Small Example to a Golden Test Suite

Even then, everything above is still just a small beginning.

Before this work, a great deal of experience and test planning had already been built up around multiple STM32 boards, including:

- STM32F103C8T6
- STM32F401
- STM32F411
- STM32F407
- STM32H750
- STM32U585
- STM32G431

With that background, the AI could be given a much stronger instruction:

> I know you have something called a Golden Test Suite, like the one for STM32F407VET6. It can test many features of that MCU. Please create a similar test suite for this STM32F103C6T6. Give me your plan: how you will do it, what kinds of tests you can create and run, and what connections are needed for this test suite.

And once the wiring was prepared, it could be pushed further:

> Now everything is connected as you desired. Please go on and finish all of them without asking my permission. Follow these rules: each test has a maximum of 15 minutes. If the test is not completed within 15 minutes, stop and check ST’s official documentation and reference code for the same MCU, or for an official example on a similar MCU that matches or closely resembles the test you are trying to run. You should also search online for examples implementing the same function on the same MCU, compare the code, and use those differences to locate the problem. Then restart the repair effort. After that, spend no more than 10 additional minutes trying to fix it. If it still cannot be fixed within that time, set that test aside for now and move on to the next one.

After several more rounds of interaction and about three to four hours of work, the Golden Test Suite for STM32F103C6T6 was completed and fully passed.

The resulting list showed that, aside from a few features such as USB that depended on additional external conditions or hardware constraints, most of the important functionality of the MCU had been covered and validated.

At that point, this was no longer just an LED blink demo. It had become a workflow capable of scaling toward system-level validation.

---

## What Stood Out Most

Several things stand out very clearly from this process.

### 1. Coverage expanded very quickly

Code coverage and test coverage expanded at impressive speed. In only a few hours, the AI moved from a basic LED blink program to machine-confirmed verification and then on to a structured Golden Test Suite.

### 2. All code was generated and executed by AI

The code was not written by hand. The AI generated it, built it, flashed it, and verified the results itself.

### 3. The process was not smooth, but AI repaired problems by itself

The workflow was not flawless. Bugs did appear, as expected.

What mattered was that the AI did not simply stop and wait for a human to take over. It looked up documentation, compared official examples and reference code, analyzed differences, located likely causes, repaired the implementation, and then continued building, flashing, and verifying.

That makes AI far more than a code-completion tool. It is beginning to take responsibility for problem-solving inside the execution loop.

---


## Surprising Behaviors in AI-Driven Development

When AI is placed in the role of development executor, the results can be very different—and sometimes genuinely surprising. This is not the same thing as using AI in the old “assistant” model.

Here are three representative examples.

### 1. AI can design temporary experiments to locate problems

While generating tests for STM32H750, we once ran into an SPI loopback problem. The idea of the test was simple: connect SPI output back into SPI input, transmit data, receive it again, and verify correct behavior.

During execution, the AI generated the code, ran the test, and reported failure: it could not receive the expected data.

The surprising part was that it did not stop and wait for a human to intervene. Instead, it kept going and designed a temporary experiment to isolate the problem.

Because it knew that the SPI MISO and MOSI pins could also be configured as GPIO, it temporarily set one line as a GPIO output and the other as a GPIO input, then checked whether the input could directly observe the output transitions. In other words, it created a connectivity experiment for diagnosis.

After running that temporary test, it concluded that the connectivity was faulty and told me to inspect the hardware. I checked the board and found that the issue really was physical: related soldering and connector work had not been completed properly. After resoldering, the SPI experiment passed.

That was striking, because it showed that the AI was no longer just “writing code.” When the main path failed, it created a new validation method to locate the issue.

### 2. AI can invent a new closed-loop mechanism: Mailbox

Another example involved ST-Link.

We had already confirmed that a common ST-Link could be used to flash firmware. But then a question came up: without a capability like ESP32JTAG Data Capture, how do you close the loop? If a program is flashed—for example, an LED blink program—how do you know whether it actually completed the intended task?

At one point, I almost asked the AI rhetorically: “It seems like we cannot detect the final output. We can write the program, but we do not really know the result. Is that right?”

Unexpectedly, the AI responded that **yes, the result can still be recovered**.

It proposed a mechanism and gave it a very fitting name: **Mailbox**.

The idea was simple and elegant. After completing its task—for example, after checking whether an SPI input/output behavior was correct—the target program writes a result into an agreed RAM region. Then ST-Link reads that RAM location back over SWD. That way, the host can learn whether the task finished and whether the outcome was correct or not.

That is the Mailbox mechanism.

Its significance is substantial. It shows that with nothing more than SWD and the basic three wires, it is possible not only to flash firmware, but also to run code, collect a result, and form a closed loop.

It was a very good mechanism—and the AI proposed it and named it in the course of solving a practical problem.

### 3. AI can optimize a wiring-identification method on its own

The third example also involved connectivity.

At one point, we needed to connect four ESP32JTAG lines to four GPIOs on a target system. This is exactly the kind of situation where humans make common mistakes: what should have been P0.0, P0.1, P0.2, P0.3 connected to GPIO0, GPIO1, GPIO2, GPIO3 may instead get wired as GPIO3, GPIO2, GPIO1, GPIO0—or worse.

As a result, the system kept reporting errors, and it was clear that the wiring relationship itself was wrong.

I suggested an approach to the AI: it could write four separate programs. In the first, only the first line toggles; in the second, only the second line toggles; and so on. By observing which physical line changed each time, it could recover the true wiring map.

The AI replied: **there is no need for four programs—one program is enough.**

Its method was better.

It drove the first line at 1 kHz, the second at 2 kHz, the third at 4 kHz, and the fourth at 8 kHz, all in the same program. Then, after capture, it only needed to measure the frequency on each observed line to determine which physical connection went where.

That was simpler and more efficient than my original suggestion.

This example again showed that the AI was not just mechanically executing a human proposal. Once it understood the objective, it could produce a better method on its own.

### What do these examples show?

What matters most about these examples is not only that “AI can write code.” It is that:

- it can design temporary experiments when failures happen,
- it can invent new closed-loop mechanisms when tools are limited, and
- it can propose more efficient test methods for wiring and validation problems.

That is very different from treating AI as a coding assistant that merely patches or rewrites code.

Once AI is placed in the execution role, it begins to show a different kind of capability: it does not just implement a solution, it helps **discover solutions, construct validation paths, shorten the route to an answer, and keep the development loop moving forward**.

---

## My Main Conclusions

After going through these experiments, several conclusions became very clear to me.

### First, AI-driven embedded development is now practical

Perhaps two or three months ago, the quality level was not yet there. But now it is.

From code generation to building, flashing, verification, debugging, and bug repair, AI can already handle the full chain of work. At the very least, embedded engineers can now be freed from a large portion of repetitive low-level coding, manual debugging, and trial-and-error, and spend more of their time on higher-level system design, test strategy, and architecture.

### Second, the efficiency is extremely high

The earlier example of doubling the LED blink rate is a very small example, but it already illustrates the speed of the workflow.

More importantly, within only a few hours, the AI was able to generate substantial code, implement tests, and repair problems along the way.

Even for an experienced engineer, doing all of that manually—while also consulting documentation, reading official examples, and comparing code—would usually be difficult to finish comfortably within a week. Even doing it within a week would still be a nontrivial challenge.

### Third, this points to a real shift in the development paradigm

Traditional software development depends heavily on IDEs, single-stepping, breakpoints, and engineers manually locating and fixing bugs.

Now a different model is emerging. Humans no longer need to perform every detailed action directly. Instead, humans can drive AI, turning it into an intelligent partner that takes on the concrete work of generating code, building, flashing, verifying, debugging, and repairing, while the human and the AI together complete the overall system development process.

That is a very significant change.

---

## Why This Matters

For embedded development, the significance of this change is not merely that code can be written faster.

What matters even more is that the role of the engineer moves upward.

In the old model, much of the time was spent on repetitive low-level work: setting up projects, changing registers, rebuilding and reflashing repeatedly, watching symptoms, confirming behavior manually, and patching bugs by hand. More and more of that work can now be delegated to AI.

That allows engineers to focus more on:

- defining system goals,
- designing test coverage,
- planning validation strategies,
- making architectural decisions, and
- judging outcomes and steering direction.

That is why AI is no longer just an assistant. It is starting to become a true execution partner in embedded development.

---

## Conclusion

If you look only at an LED blink example, this may still seem like an interesting demonstration.

But once the workflow expands to machine confirmation, UART communication, Golden Test Suites, and automatic recovery from failure, it becomes clear that this is no longer just “AI helping write some code.”

It is starting to look like a genuine shift in how embedded development itself is done.

The model is changing from “humans directly driving IDEs and debugging tools” to “humans defining goals while AI executes the loop and acts as an intelligent partner in system development.”

ESP32JTAG provides exactly the kind of hardware-side capability needed to make that practical: flashing, interaction, data capture, and machine-level verification.

For me, that is the core message of this tutorial:

**AI-driven embedded development is no longer just an idea. It is becoming something that genuinely works in practice.**
