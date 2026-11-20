# Interactive To-Do Lists for CrossPoint

*Not just a list to look at: write, tick and change it right on your e-reader.*

To-do and shopping lists for the [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader)
firmware.

![A shopping list on the Xteink X4 Pro](images/list.jpg)

- **Create, edit, delete** – right on the reader
- **Several entries at once**
- **Tick & undo**
- **Smart lists** – nothing is added twice
- **Hide / show checked**
- **Swipe between lists**
- **Notepad look & covers**
- **Plain text files** – no app, no cloud
- *Optional:* **send lists from phone or PC**

**Free & open source · Ready to flash · Updates built in**

## Install

Get the file for your device from **[Releases](../../releases)**:

| Device | File |
| --- | --- |
| Xteink X4 Pro | `crosspoint-<version>-x4pro.bin` |
| Xteink X3 and X4 | `crosspoint-<version>-x3-x4.bin` |

> **X3 and X4: not tested.** No touch screen there, so everything runs over the buttons, which
> makes the lists harder to use.

Copy it to the SD card and use **Settings → SD Card Firmware Update**. Later updates:
**Settings → Check for Updates**.

## Make and use lists right on the reader

No phone or computer needed: create lists, write, tick and delete everything on the reader.
An empty list, **TO DO**, is there from the start: open it like a book and write. Tap its
headline for **new list**. There is always at least one list: if you delete the last one (anywhere), a
new empty **TO DO** appears.

| What | X4 Pro (touch) | X3 / X4 (buttons) |
| --- | --- | --- |
| Tick | Tap the box | Left / Right, **Confirm** |
| Turn the page, next / previous list | Swipe (as in Settings) | Side keys |
| Add entries | Tap an empty line | Hold **Confirm** → menu |
| Edit or delete an entry | Hold the entry | Hold **Confirm** → menu |
| Check all, uncheck all, delete all | Hold a box | Hold **Confirm** → menu |
| New list, rename, delete list | Tap the headline | Hold **Confirm** → menu |
| Hide / show checked | Reader-menu gesture | Hold **Confirm** → menu |

- `,` or `;` separate entries: `Milk, Bread; Cheese` is three. For a comma inside an entry,
  use a dash: `Apples - the red ones`.
- Turning past the last page opens the next list, turning back before the first page the
  previous one (lists in name order).
- The home screen shows one list, the one used last, with a picture of the list. From
  there, swipe through all your lists. The picture is made once; to update it, open
  **rename list** and confirm the name.
- A deleted entry can be brought back with **undo** in the bottom line. Deleting a whole list
  asks first.

## Write a list on your computer

<details>
<summary>Show</summary>

A list is a simple text file, saved as `.txt` (e.g. `Shopping.txt`). Write it the way you like:

One thing per line:

```
Milk
Bread
Butter
```

Or all in one line:

```
Milk, Bread, Butter
```

Both give the same three entries. Dashes, stars or numbers in front (`- Milk`, `1. Milk`) are
fine too.

Put the file into the folder **`Lists`** on the SD card. That's all: the reader shows it as a
list.

Good to know:
- For a comma that belongs to the thing, use a dash: `Apples - the red ones`.
- The order stays as you wrote it.
- What you tick, the reader notes in the file as `[x]`.

</details>

## Send from your phone or computer

<details>
<summary>Show</summary>

**First, every time:** on the reader, start **File Transfer**. Phone or computer and reader must
be in the same Wi-Fi (or use the reader's **Create Hotspot**).

**Easiest way, no setup:** open the address the reader shows in your browser, go to the
`Lists` folder and upload your `.txt` file. (A file with the same name replaces that list.)

**Share button** (set up once, then two taps): pick your device below.

<details>
<summary>iPhone, iPad and Mac</summary>

1. Open the **Shortcuts** app, tap **+**, name it **To X4**.
2. Add the action **Get Contents of URL** and enter `http://crosspoint.local/api/lists/add`
3. Tap **Show More**: Method **POST**, Request Body **Form**, add two text fields:
   - `list` → tap the value, choose **Ask Each Time**
   - `text` → tap the value, choose **Shortcut Input**
4. Tap **ⓘ** and turn on **Show in Share Sheet**.

Now select some text anywhere, tap **Share → To X4** and type the list name. The things are
added to that list (or a new list is made). Nothing is added twice.

If it doesn't work, replace `crosspoint.local` with the address the reader shows (e.g. `192.168.0.25`).

</details>

<details>
<summary>Android</summary>

1. Install the free app [HTTP Shortcuts](https://http-shortcuts.rmy.ch).
2. Create two variables: **text** (type *Text input*, turn on *Allow receiving value from Share
   dialog*) and **list** (type *Text input*).
3. Create a shortcut **To X4**: method **POST**, URL `http://crosspoint.local/api/lists/add`,
   request body *Form*: `list` = `{list}`, `text` = `{text}`.

Then: select text → **Share → To X4** → type the list name.

If it doesn't work, replace `crosspoint.local` with the address the reader shows (e.g. `192.168.0.25`).

</details>

<details>
<summary>For tinkerers</summary>

```
curl -d "list=Shopping" --data-urlencode "text=Milk, Bread" http://crosspoint.local/api/lists/add
```

`GET /api/lists` lists all lists. Like all of CrossPoint's File Transfer there is no password:
use it only in your own Wi-Fi or the reader's hotspot. It only ever writes into `/Lists`.

</details>

</details>

## Why a firmware and not a plugin?

The lists can't be a CrossPoint plugin: they need to hook into the reader itself, the home screen
and File Transfer, and plugins can't reach those parts. So they are built into the latest
**official** CrossPoint: as soon as a new CrossPoint version is out, a new build follows, usually
within a day. You never fall behind official CrossPoint.

## More add-ons

- [Crosspoint-Dogear](https://github.com/Menrix/Crosspoint-Dogear): bookmarks show as a folded
  page corner.
- [Crosspoint-Addons](https://github.com/Menrix/Crosspoint-Addons): **all my add-ons in one
  firmware**. A growing collection; install this one if you want more than one of them.

## How it is built

Every night a GitHub Action adds this to the latest official CrossPoint release and publishes
the firmware under Releases.

## License

MIT, like CrossPoint. Cyrillic letters in the list font: [Pangolin](https://fonts.google.com/specimen/Pangolin) © The Pangolin Project Authors, SIL Open Font License 1.1 (see `licenses/Pangolin-OFL.txt`). Font [Patrick Hand](https://fonts.google.com/specimen/Patrick+Hand)
© Patrick Wagesreiter, SIL Open Font License 1.1.

Made with the help of Claude.
