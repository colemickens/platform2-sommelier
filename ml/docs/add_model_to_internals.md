# How to add a tab for a new model in `chrome://machine-learning-internals`

The internals page `chrome://machine-learning-internals` contains tools for
controlling the ML Service (connecting, disconnecting) and evaluating a subset
of models for benchmarking or debugging purposes. To make a new model available
here, follow the instructions below.

## Use cases & deciding whether its worthwhile

Adding your model to the internals Web UI page represents a maintenance burden
in a couple of ways:

1. If your model inputs change, the way you invoke it from the internals page
will need to be updated just as with other calls to the model.
2. It increases the overall maintenance burden for changes to the Web UI
framework overall.

You should only do this if the benefits will outweigh the costs. Here's a
discussion of some use cases:

* Just evaluating the model with arbitrary input values: This can probably be
done more easily offline with e.g. Python TF Lite directly.
* Evaluating the model with inputs derived from the real Chrome / Chrome OS
environment: First consider whether it may be easier to capture and export the
relevant input data somehow, to avoid maintaining Web UI.
* Evaluating model performance (latency / throughput / resource usage) with
specific inputs and/or to debug a specific device or device conditions
(memory/CPU pressure): First consider whether the ML Service histograms
described in [README.md] can give you the same information.

## Instructions

Basically, you need add some html code of your tab to
[machine_learning_internals.html],  and create a `your_model_tab.js` file in
in [chrome/browser/resources/chromeos/machine_learning/]. The html code defines
the user interface for input and output, and `your_model_tab.js`
defines the behaviour, like which model id to load, how to compose input tensors
and how to extract information from output tensors.

Concretely, in `<tabbox>` section of [machine_learning_internals.html], you need
to add a `<tab>` to `<tabs>` as the title, add a `<tabpanel>` in
`<tabpanels>` as the main part of your tab, then import the `your_model_tab.js`
file, like:

```html
<tabbox>
  <tabs>
    <tab>Your Model</tab>
  </tabs>
  <tabpanels>
    <tabpanel>
      <!-- elements of the new tab -->
    </tabpanel>
  </tabpanels>
</tabbox>
<script src="your_model_tab.js"></script>
```

You can take [test_model_tab] and [test_model_tab.js] as an example.

Then you need to register the new-added `your_model_tab.js` file in
[browser_resources.grd] by adding an entry for it, like

```xml
<include name="IDR_MACHINE_LEARNING_INTERNALS_YOUR_MODEL_TAB_JS" file="resources\chromeos\machine_learning\your_model_tab.js" type="BINDATA" compress="gzip" />
```

After that, add it to [machine_learning_internals_ui.cc], like

```c++
source->AddResourcePath("your_model_tab.js",
                        IDR_MACHINE_LEARNING_INTERNALS_YOUR_MODEL_TAB_JS);
```

## Constructing input tensor

If constructing input for your model requires some complicated operation which
is difficult for javascript, and you want to do it in cpp, you can modify the
[machine_learning_internals_page_handler.mojom] file, add your interface
function, and implement it in [machine_learning_internals_page_handler.h] and
[machine_learning_internals_page_handler.cc]. Then you can call this interface
via `pageHandler` in [machine_learning_internals.js], execute this complicate
operation in cpp and get the result in javascript.

[README.md]: ../README.md
[machine_learning_internals.html]: https://cs.chromium.org/chromium/src/chrome/browser/resources/chromeos/machine_learning/machine_learning_internals.html
[chrome/browser/resources/chromeos/machine_learning/]: https://cs.chromium.org/chromium/src/chrome/browser/resources/chromeos/machine_learning/
[test_model_tab]: https://cs.chromium.org/chromium/src/chrome/browser/resources/chromeos/machine_learning/machine_learning_internals.html?q=machine_learning_internals&dr=C&l=30
[test_model_tab.js]: https://cs.chromium.org/chromium/src/chrome/browser/resources/chromeos/machine_learning/test_model_tab.js
[machine_learning_internals.html]: https://cs.chromium.org/chromium/src/chrome/browser/resources/chromeos/machine_learning/machine_learning_internals.html
[browser_resources.grd]: https://cs.chromium.org/chromium/src/chrome/browser/browser_resources.grd?l=725
[machine_learning_internals_ui.cc]: https://cs.chromium.org/chromium/src/chrome/browser/ui/webui/chromeos/machine_learning/machine_learning_internals_ui.cc
[machine_learning_internals_page_handler.mojom]: https://cs.chromium.org/chromium/src/chrome/browser/ui/webui/chromeos/machine_learning/machine_learning_internals_page_handler.mojom
[machine_learning_internals_page_handler.h]: https://cs.chromium.org/chromium/src/chrome/browser/ui/webui/chromeos/machine_learning/machine_learning_internals_page_handler.h
[machine_learning_internals_page_handler.cc]: https://cs.chromium.org/chromium/src/chrome/browser/ui/webui/chromeos/machine_learning/machine_learning_internals_page_handler.cc
[machine_learning_internals.js]: https://cs.chromium.org/chromium/src/chrome/browser/resources/chromeos/machine_learning/machine_learning_internals.js
