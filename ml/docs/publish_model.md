# ChromeOS ML Service: How to publish your ML models

This page explains how to publish a trained ML model so that the ML Service
can make it available at runtime for inference.
Design doc for model publishing: [go/cros-ml-service-models].

[TOC]

## Model format

Currently the ML Service supports **TensorFlow Lite** models.
Each model needs to be a single file.
The file format is a TFLite flat buffers (extension .tflite).

Handy tool: you can convert a TF model into a TFLite model using the internal
tool Toco (see [go/toco]).

## Two methods to publish your models

Two methods will be supported to publish an ML model:

1. Installed into rootfs partition.
2. Installed into the user partition as a downloadable component by DLC
Service.

> WARNING: Currently only the first method is supported. DLC is still under
> development and its ETA is Q4 2018.

To know more about which method you should use and why we have two, read
[section below](#two-methods-why).
In general, **method #2 is the preferred option**, unless you know that you need
method #1 and you have received permission to increase the storage size of
rootfs. Remember that storage in rootfs is limited and we are trying to control
rootfs growth as much as possible.

## Method #1: Install a model inside rootfs

### Step 1. Upload the model to the ChromeOS file mirror

Once you have a model ready that you want to publish, upload it to
https://storage.googleapis.com/chromeos-localmirror/distfiles/.
The web interface is
https://pantheon.corp.google.com/storage/browser/chromeos-localmirror/distfiles.
The mirror is accessible to all Googlers with prod access.

There is no directory structure under `distfiles/`, so the filename needs to be
unique enough to avoid conflicts.
This is the **required convention** for filenames of models:

```
mlservice-model-<feature_name_and_variant>-<timestamp>.<ext>
```

* `mlservice-model` is a fixed prefix
* `<feature_name_and_variant>` should indicate the feature that this model is
built for and any variant of the model if applicable
* `<timestamp>` is used to differentiate different versions of the same
model. The preferred format is `20180725` but the actual time (hour, minutes,
etc) can be added if needed.

An example of filename is:
`mlservice-model-tab_discarder_quantized-20180507.pb`.

Files in the ChromeOS file mirror should never be deleted. You simply add newer
models as you need them, but leave the previous ones in the mirror, even if you
don't plan to use them ever again.

### Step 2. Write an ebuild to install the model into rootfs

Once your model is uploaded to the ChromeOS file mirror, the next step is to
create a CL.
This CL will have changes to the ML ebuild, which installs the model(s) into
rootfs.
The ebuild is located at
/chromiumos/overlays/chromiumos-overlay/chromeos-base/ml/ml-9999.ebuild.
Simply add your models in the `system_models` variable: they are installed in the
`src_install()` function in the same file.
The install location in the ChromeOS system is `/opt/google/chrome/ml_models`.
See [CL/1125701] (and relative fix in [CL/1140020]) as an example.

### Step 3. Update ML Service daemon to serve the new model

There are three places to update here:

1. Add the model to the Mojo interface in [model.mojom].
2. Add a metadata entry to [model_metadata.cc].
3. Add a basic [loading & inference test] for the model.

See [this CL](https://crrev.com/c/1342736) for an example of all the above.

## Method #2: Use DLC to install a model inside the stateful partition

> WARNING: This section is pending the implementation of DLC Service.
> See [go/dlc-service-proposal] for more info.


## Which method to use and why {#two-methods-why}
We provide two methods of model deployment, each with their own pros and cons.

Method #1, installing inside rootfs, has the benefit of making the model
available right after the system boots for the first time.
No updates and thus no network connection is required.
Besides, it was better suited as a starting point to create a working prototype
of the ML Service in the early stages of development.

Method #1 has one big limit: storage space on rootfs. Since we want to avoid
increasing the size of rootfs, we reserve this method for those models that are
either very small (less than 30KB) or that are system critical and need to be
available before the first successful update of the device.

Method #2 is the solution to the limitation of Method #1. Large models can be
installed this way, because we have no restrictions on the size increase of the
stateful partition (within reason).
Models are installed in an encrypted partition. Files in that partition are
shared by all users.

Method #2 promises to bind models to platform releases, so all models can be
verified and tested automatically before they are sent to device.

The disadvantage of Method #2 is that the model cannot be used until
the system has succesfully updated.

If you believe that your case is eligible for Method #1, contact
chrome-knowledge-eng@ to get your case approved.

[CL/1125701]: http://crrev.com/c/1125701
[CL/1140020]: http://crrev.com/c/1140020
[go/cros-ml-service-models]: http://go/cros-ml-service-models
[go/dlc-service-proposal]: http://go/dlc-service-proposal
[go/toco]: http://go/toco
[loading & inference test]: https://cs.corp.google.com/chromeos_public/src/platform2/ml/machine_learning_service_impl_test.cc
[model.mojom]: https://cs.corp.google.com/chromeos_public/src/platform2/ml/mojom/model.mojom
[model_metadata.cc]: https://cs.corp.google.com/chromeos_public/src/platform2/ml/model_metadata.cc
