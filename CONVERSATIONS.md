# LiteRT-LM Unreal: Single and Multiple Conversations

**English** | [中文](CONVERSATIONS_zh.md)

This guide targets the current **Unreal Engine 5.8 / LiteRT-LM v0.14 / Stable ABI 1.2** plugin. Node names and pins were verified in the UE 5.8 Editor.

## One principle first

`ULiteRtLmQuickChat` is not a second conversation system. It is a compatibility facade over an ordinary, default-configured `ULiteRtLmAgent` (Conversation):

```text
Create Quick Chat
  -> creates one ordinary Agent / Conversation
  -> applies default configuration
  -> forwards chunks, completion, error, and state events
```

Consequently:

- Single and multiple conversations share the same memory, queue, cancellation, and lifetime implementation.
- Every `Create Quick Chat` call creates an independent ordinary Conversation; it is not a process-wide singleton.
- `Get Conversation` returns the underlying `ULiteRtLmAgent`, so you can adopt memory, tools, and persistence APIs without losing context.
- One process loads one shared model. Conversations keep independent canonical histories while the Subsystem serializes GPU requests.

## 0. Project setup

1. Enable **LiteRT-LM-Unreal** under `Edit > Plugins`, then restart the Editor.
2. Open `Project Settings > Plugins > LiteRT-LM`.
3. Set `Model Path`, context size, and default sampling settings.
4. Do not look for a `Load Model` node. Creating the first Quick Chat or Conversation starts lazy loading automatically.

## 1. Quick single conversation (Blueprint)

Use this for one chat window, one assistant, a tutorial NPC, or any feature with one continuous history.

### Step 1: create and retain the object

In a Blueprint with an appropriate lifetime, such as a Player Controller, Game Instance subsystem, or long-lived chat Widget owner:

1. Call `Create Quick Chat` from `BeginPlay` or your initialization event.
2. Enter a stable role description in `System Prompt`, or leave it empty.
3. **Promote Return Value to a variable** named `QuickChat`.

Do not call `Create Quick Chat` for every message. A new object is a new conversation.

### Step 2: bind result events

Bind these events from the `QuickChat` variable:

- `On Answer`: the complete `FLiteRtLmResult` for every accepted request.
- `On Error`: structured configuration or request errors.
- `On Text Chunk`: only when you need a typewriter UI.
- `On State Changed`: useful for disabling Send or showing loading/generation status.

Events are delivered on the Game Thread and may update UMG directly.

### Step 3: choose one ask mode

Your Send action should call exactly one node:

- `Ask Once(Message, Options)` suppresses chunks and publishes the final result through `On Answer`.
- `Ask Streaming(Message, Options)` publishes `On Text Chunk` while generating and still emits one final `On Answer`.

The `Return Value` from both nodes is a **Request Id**, not the answer. A non-empty Id means the request was accepted; the answer is asynchronous.

### Step 4: continue the same context

Call `Ask Once` or `Ask Streaming` again on the same `QuickChat` variable. The underlying Agent retains prior user and assistant messages.

### Step 5: reset, cancel, or close

- New topic: `Reset Conversation(true)` clears history and preserves the System Prompt.
- Stop current generation: `Cancel`.
- Owner is permanently leaving: `Close`, then clear your reference.
- Need advanced APIs: `Get Conversation` returns the same underlying Agent.

### UE 5.8-verified single-conversation nodes

| Node | Main inputs | Output/event |
| --- | --- | --- |
| `Create Quick Chat` | `System Prompt` | `Lite Rt Lm Quick Chat` |
| `Ask Once` | `Message`, `Options` | Request Id; final result through `On Answer` |
| `Ask Streaming` | `Message`, `Options` | Request Id; chunks through `On Text Chunk`, final result through `On Answer` |
| `Get Conversation` | Quick Chat | The same `Lite Rt Lm Agent` |
| `Reset Conversation` | `Keep System Prompt` | Success |

## 2. Quick single conversation (C++)

Add `LiteRTLMUnreal` to your module dependencies and retain the object with `UPROPERTY`:

```cpp
#include "LiteRtLmBlueprintLibrary.h"
#include "LiteRtLmQuickChat.h"

UPROPERTY(Transient)
TObjectPtr<ULiteRtLmQuickChat> QuickChat;

UFUNCTION()
void HandleAnswer(const FLiteRtLmResult& Result);

UFUNCTION()
void HandleChunk(const FString& RequestId, const FString& TextChunk);

UFUNCTION()
void HandleChatError(const FLiteRtLmError& Error);
```

Create it once during initialization:

```cpp
QuickChat = ULiteRtLmBlueprintLibrary::CreateQuickChat(
    this,
    TEXT("Answer clearly and remember the ongoing conversation."));

if (QuickChat)
{
    QuickChat->OnAnswer.AddDynamic(this, &ThisClass::HandleAnswer);
    QuickChat->OnTextChunk.AddDynamic(this, &ThisClass::HandleChunk);
    QuickChat->OnError.AddDynamic(this, &ThisClass::HandleChatError);
}
```

Use the default-options C++ overload when sending:

```cpp
const FString RequestId = QuickChat
    ? QuickChat->AskStreaming(TEXT("Remember that my codename is Orion."))
    : FString();
```

Blueprint retains one unambiguous reflected signature. C++ additionally gets convenience overloads that omit `FLiteRtLmAskOptions`.

## 3. Multiple conversations (Blueprint)

Use this for multiple NPCs, Werewolf, party members, chat tabs, or workflows that require isolated memories.

### Step 1: prepare a container

Create an array of `Lite Rt Lm Agent Object Reference`, for example `Conversations`. If characters have stable IDs, also maintain a `Role Id -> Agent` map.

### Step 2: create one Conversation per character

At match or scene initialization, repeat for every character:

1. Use `Make Lite Rt Lm Agent Config`.
2. Set `Display Name`, the character-specific `System Prompt`, and optional `Tool Declarations Json`.
3. Call `Create Conversation (Advanced)`.
4. Add the returned Agent to `Conversations`.

Werewolf should use one Agent per AI player—not one shared Agent for every player, and not a new Agent every turn.

### Step 3: bind each Agent's events

Bind `On Text Chunk`, `On Completed`, `On Error`, and optionally `On State Changed`. When handlers are shared, use the owner, Agent Id, or your request map to identify which character completed the request.

### Step 4: ask the character whose turn it is

Select the current character's Agent from the array or map, then call `Ask(Message, Options)`. Do not submit again while that Agent is busy. Requests from different Agents are still serialized by the shared GPU queue.

### Step 5: synchronize only facts a character actually knows

- Use `Append Memory Message to Conversations` for an event every selected recipient may know.
- Use one Agent's `Append Memory Message` for private information.
- Record the event itself, for example `Seat 3 said: “I am a werewolf.”` Do not append prompt boilerplate such as “this information is public and you must remember it.” Game rules decide which Agents receive the event.

### Step 6: validate tools and rejected actions

Game rules remain authoritative:

1. Read structured actions from `Result.ToolCalls` in `On Completed`.
2. Validate seat, phase, role, and target.
3. Valid and no continuation required: `Append Tool Result to Memory`.
4. Valid and the model should continue: `Submit Tool Result`.
5. Invalid answer and a retry is required: call `Reject Last Response`, then ask again. This prevents the invalid assistant action from becoming a bad example in memory.

### Step 7: retire the scene

Call `Close and Clear Conversations` on the array. It closes every unique valid Agent and clears the caller-owned array so callbacks from the old match cannot enter the new one.

### UE 5.8-verified multiple-conversation nodes

| Node | Purpose |
| --- | --- |
| `Create Conversation (Advanced)` | Creates one independent Conversation from `Agent Config` |
| `Ask` | Queues a request on one Agent and returns its Request Id |
| `Append Memory Message to Conversations` | Writes one real event to a selected group of Agents |
| `Close and Clear Conversations` | Closes Agents and clears the array |
| `Export/Import/Save/Load Memory` | Inspection, migration, and persistence |
| `Reject Last Response` | Removes the terminal assistant reply while retaining its input |

## 4. Multiple conversations (C++)

```cpp
#include "LiteRtLmAgent.h"
#include "LiteRtLmSubsystem.h"

UPROPERTY(Transient)
TArray<TObjectPtr<ULiteRtLmAgent>> Conversations;

ULiteRtLmSubsystem* Runtime =
    GEngine->GetEngineSubsystem<ULiteRtLmSubsystem>();

ULiteRtLmAgent* Seat3 = Runtime
    ? Runtime->CreateAgent(
        TEXT("Seat 3"),
        TEXT("You are seat 3 in a social deduction game. Follow your role and the game rules."))
    : nullptr;

if (Seat3)
{
    Conversations.Add(Seat3);
    Seat3->OnCompleted.AddDynamic(this, &ThisClass::HandleAgentCompleted);
    Seat3->OnError.AddDynamic(this, &ThisClass::HandleAgentError);
    Seat3->Ask(TEXT("Give your current analysis."));
}
```

These shorter `CreateAgent` and `Ask` signatures are C++ convenience overloads over the same Agent implementation used by Blueprint.

## 5. Memory-correctness checklist

When an AI appears to ignore history, check in this order:

1. Is Quick Chat/Agent recreated before every message?
2. Are multiple characters incorrectly sharing one Agent?
3. Was every fact visible to this character written to its Agent, rather than only shown in UI?
4. Did `Ask` return a non-empty Request Id? An empty Id means the request was not accepted.
5. Is memory being changed, or another request submitted, while the Agent is busy?
6. Inspect canonical history with `Export Conversation Json` / `Export Memory Json` before the next request.
7. After game rules reject a tool action, was `Reject Last Response` called?

Physical KV is not public state. On the current Win64 GPU runtime, consecutive requests to the same Agent can reuse the resident KV. Switching Agents rebuilds context from that Agent's complete canonical history. Semantics are preserved, while cross-Agent switching may add prefill cost.

## 6. Which entry should I use?

| Requirement | Entry |
| --- | --- |
| One chat window, fastest path to an answer | `Create Quick Chat` |
| One conversation plus persistence, tools, or precise memory edits | Quick Chat + `Get Conversation` |
| Multiple NPCs, players, or tabs | One `Create Conversation (Advanced)` per character |
| An Actor owns a long-lived AI | `LiteRtLmComponent` |
| Fixed tool schema with external MCP routing | `Create MCP Gateway` |
