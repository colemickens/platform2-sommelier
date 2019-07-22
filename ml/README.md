# Chrome OS Machine Learning Service

## Summary

The Machine Learning (ML) Service provides a common runtime for evaluating
machine learning models on device. The service wraps the TensorFlow Lite runtime
and provides infrastructure for deployment of trained models. The TFLite runtime
runs in a sandboxed process. Chromium communicates with ML Service via a Mojo
interface.

## How to use ML Service

You need to provide your trained models to ML Service by following [these
instructions](docs/publish_model.md). You can then load and use your model from
Chromium using the client library provided at
[//chromeos/services/machine_learning/public/cpp/]. Optional: Later, if you find
a need for it, you can [add your model to the ML Service internals
page](docs/add_model_to_internals.md).

Note: The sandboxed process hosting TFLite models is currently shared between
all users of ML Service. If this isn't acceptable from a security perspective
for your model, follow [this bug](http://crbug.com/933017) about switching ML
Service to having a separate sandboxed process per loaded model.

## Metrics

The following metrics are currently recorded by the daemon process in order to
understand its resource costs in the wild:

* MachineLearningService.MojoConnectionEvent: Success/failure of the
  D-Bus->Mojo bootstrap.
* MachineLearningService.TotalMemoryKb: Total (shared+unshared) memory footprint
  every 5 minutes.
* MachineLearningService.PeakTotalMemoryKb: Peak value of
  MachineLearningService.TotalMemoryKb per 24 hour period. Daemon code can
  also call ml::Metrics::UpdateCumulativeMetricsNow() at any time to take a
  peak-memory observation, to catch short-lived memory usage spikes.
* MachineLearningService.CpuUsageMilliPercent: Fraction of total CPU resources
  consumed by the daemon every 5 minutes, in units of milli-percent (1/100,000).

Additional metrics added in order to understand the resource costs of each
request:

* MachineLearningService.|request|.Event: OK/ErrorType of the request.
* MachineLearningService.|request|.TotalMemoryDeltaKb: Total (shared+unshared)
  memory delta caused by the request.
* MachineLearningService.|request|.ElapsedTimeMicrosec: Time cost of the
  request.
* MachineLearningService.|request|.CpuTimeMicrosec: CPU time usage of the
  request, which is scaled to one CPU core, namely it may be greater than
  ElapsedTimeMicrosec if there are multi cores.

The above request can be following:

* LoadModel
* CreateGraphExecutor
* Execute (model inference)

## Original design docs

Note that aspects of the design may have evolved since the original design docs
were written.

* [Overall design](https://docs.google.com/document/d/1ezUf1hYTeFS2f5JUHZaNSracu2YmSBrjLkri6k6KB_w/edit#)
* [Mojo interface](https://docs.google.com/document/d/1pMXTG-OIhkNifR2DCPa2bCF0X3jrAM-U6UK230pBv5I/edit#)
* [Deamon\<-\>Chromium IPC implementation](https://docs.google.com/document/d/1EzBKLotvspe75GUB0Tdk_Namstyjm6rJHKvNmRCCAdM/edit#)
* [Model publishing](https://docs.google.com/document/d/1LD8sn8rMOX8y6CUGKsF9-0ieTbl97xZORZ2D2MjZeMI/edit#)

[//chromeos/services/machine_learning/public/cpp/]: https://cs.chromium.org/chromium/src/chromeos/services/machine_learning/public/cpp/service_connection.h
