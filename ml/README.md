# CrosML: Chrome OS Machine Learning Service

## Summary

The Machine Learning (ML) Service provides a common runtime for evaluating
machine learning models on device. The service wraps the TensorFlow Lite runtime
and provides infrastructure for deployment of trained models. Chromium
communicates with ML Service via a Mojo Interface.

## How to use ML Service

You need to provide your trained models to ML Service by following [these
instructions](docs/publish_model.md).
You can then load and use your model from Chromium using the client library
provided at [//chromeos/services/machine_learning/public/cpp/].

## Metrics

The following metrics are currently recorded by the daemon process in order to
understand its resource costs in the wild:

* MachineLearningService.MojoConnectionEvent: Success/failure of the
  D-Bus->Mojo bootstrap.
* MachineLearningService.PrivateMemoryKb: Private (unshared) memory footprint
  every 5 minutes.
* MachineLearningService.PeakPrivateMemoryKb: Peak value of
  MachineLearningService.PrivateMemoryKb per 24 hour period. Daemon code can
  also call ml::Metrics::UpdateCumulativeMetricsNow() at any time to take a
  peak-memory observation, to catch short-lived memory usage spikes.
* MachineLearningService.CpuUsageMilliPercent: Fraction of total CPU resources
  consumed by the daemon every 5 minutes, in units of milli-percent (1/100,000).

Additional metrics added in order to understand the resource costs of each
request:

* MachineLearningService.|request|.Event: OK/ErrorType of the request.
* MachineLearningService.|request|.PrivateMemoryDeltaKb: Private (unshared)
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

## Design docs

* Overall design: [go/chromeos-ml-service]
* Mojo interface: [go/chromeos-ml-service-mojo]
* Deamon\<-\>Chromium IPC implementation: [go/chromeos-ml-service-impl]
* Model publishing: [go/cros-ml-service-models]


[go/chromeos-ml-service]: http://go/chromeos-ml-service
[go/chromeos-ml-service-mojo]: http://go/chromeos-ml-service-mojo
[go/chromeos-ml-service-impl]: http://go/chromeos-ml-service-impl
[go/cros-ml-service-models]: http://go/cros-ml-service-models
[//chromeos/services/machine_learning/public/cpp/]: https://cs.chromium.org/chromium/src/chromeos/services/machine_learning/public/cpp/service_connection.h
