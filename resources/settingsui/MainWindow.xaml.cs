using ConfigEditor.Models;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using System.Collections.ObjectModel;
using System.IO;
using System.Reflection;
using System.Windows;
using MahApps.Metro.Controls;

namespace ConfigEditor
{
	public partial class MainWindow : MetroWindow
	/// <summary>
	/// Interaction logic for MainWindow.xaml
	/// </summary>
    {
        public ObservableCollection<Base> ProcessorType { get; set; }
        public ObservableCollection<Base> LogConfig { get; set; }
        public ObservableCollection<Base> LogGeneral { get; set; }
        public ObservableCollection<Base> LogMiner { get; set; }
        public ObservableCollection<Base> LogNonceSubmitter { get; set; }
        public ObservableCollection<Base> LogPlotReader { get; set; }
        public ObservableCollection<Base> LogPlotVerifier { get; set; }
        public ObservableCollection<Base> LogServer { get; set; }
        public ObservableCollection<Base> LogSession { get; set; }
        public ObservableCollection<Base> LogSocket { get; set; }
        public ObservableCollection<Base> LogWallet { get; set; }

        public Base ProcessorTypeSelected { get; set; }

        public ObservableCollection<Base> CPUInstSet { get; set; }
        public Base CpuSelected { get; set; }

        public JObject Config { get; set; }

        public MainWindow()
        {
			InitializeComponent();
            string executableLocation = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
            string path = Path.Combine(executableLocation, "mining.conf");
            var json = File.ReadAllText(path);

            Config = JsonConvert.DeserializeObject<JObject>(json);
            ProcessorTypeSelected = new Base("CPU");

            miningInfo.Text = Config["mining"]["urls"]["miningInfo"].ToString();
			submission.Text = Config["mining"]["urls"]["submission"].ToString();
			wallet.Text = Config["mining"]["urls"]["wallet"].ToString();

            if (submission.Text == wallet.Text)
            {
                chPool.IsChecked = true;
            }

			bufferChunkCount.Text = Config["mining"]["bufferChunkCount"].ToString();
			getMiningInfoInterval.Text = Config["mining"]["getMiningInfoInterval"].ToString();
			gpuDevice.Text = Config["mining"]["gpuDevice"].ToString();

			gpuPlatform.Text = Config["mining"]["gpuPlatform"].ToString();
			intensity.Text = Config["mining"]["intensity"].ToString();
			maxBufferSizeMB.Text = Config["mining"]["maxBufferSizeMB"].ToString();

			maxPlotReaders.Text = Config["mining"]["maxPlotReaders"].ToString();
			submissionMaxRetry.Text = Config["mining"]["submissionMaxRetry"].ToString();
			submitProbability.Text = Config["mining"]["submitProbability"].ToString();

			targetDeadline.Text = Config["mining"]["targetDeadline"].ToString();
			timeout.Text = Config["mining"]["timeout"].ToString();
			wakeUpTime.Text = Config["mining"]["wakeUpTime"].ToString();

			walletRequestRetryWaitTime.Text = Config["mining"]["walletRequestRetryWaitTime"].ToString();
			walletRequestTries.Text = Config["mining"]["walletRequestTries"].ToString();

			chRescanEveryBlock.IsChecked = Config["mining"]["rescanEveryBlock"].ToObject<bool>();
			chUseInsecurePlotfiles.IsChecked = Config["mining"]["useInsecurePlotfiles"].ToObject<bool>();

			chBenchmark.IsChecked = Config["mining"]["benchmark"]["active"].ToObject<bool>();
			interval.Text = Config["mining"]["benchmark"]["interval"].ToString();

			chStart.IsChecked = Config["webserver"]["start"].ToObject<bool>();
			activeConnections.Text = Config["webserver"]["activeConnections"].ToString();

			chForwardMinerNames.IsChecked = Config["webserver"]["forwardMinerNames"].ToObject<bool>();
			chCumulatePlotsizes.IsChecked = Config["webserver"]["cumulatePlotsizes"].ToObject<bool>();
			chCalculateEveryDeadile.IsChecked = Config["webserver"]["calculateEveryDeadline"].ToObject<bool>();

			url.Text = Config["webserver"]["url"].ToString();
			user.Text = Config["webserver"]["credentials"]["user"].ToString();
			pass.Text = Config["webserver"]["credentials"]["pass"].ToString();

			chLogfile.IsChecked = Config["logging"]["logfile"].ToObject<bool>();
			txtFirst.Text = Config["logging"]["path"].ToString();

			chuseColors.IsChecked = Config["logging"]["useColors"].ToObject<bool>();
			chFancy.IsChecked = Config["logging"]["progressBar"]["fancy"].ToObject<bool>();
			chSteady.IsChecked = Config["logging"]["progressBar"]["steady"].ToObject<bool>();

			this.LogConfig = new ObservableCollection<Base>();
            this.LogGeneral = new ObservableCollection<Base>();
            this.LogMiner = new ObservableCollection<Base>();
            this.LogNonceSubmitter = new ObservableCollection<Base>();
            this.LogPlotReader = new ObservableCollection<Base>();
            this.LogPlotVerifier = new ObservableCollection<Base>();
            this.LogServer = new ObservableCollection<Base>();
            this.LogSession = new ObservableCollection<Base>();
            this.LogSocket = new ObservableCollection<Base>();
            this.LogWallet = new ObservableCollection<Base>();
            this.CPUInstSet = new ObservableCollection<Base>();
            this.ProcessorType = new ObservableCollection<Base>();

            this.LogConfig.Add(new Base("off"));
            this.LogConfig.Add(new Base("fatal"));
            this.LogConfig.Add(new Base("critical"));
            this.LogConfig.Add(new Base("error"));
            this.LogConfig.Add(new Base("warning"));
            this.LogConfig.Add(new Base("notice"));
            this.LogConfig.Add(new Base("information"));
            this.LogConfig.Add(new Base("debug"));
            this.LogConfig.Add(new Base("trace"));
            this.LogConfig.Add(new Base("all"));

            this.LogGeneral.Add(new Base("off"));
            this.LogGeneral.Add(new Base("fatal"));
            this.LogGeneral.Add(new Base("critical"));
            this.LogGeneral.Add(new Base("error"));
            this.LogGeneral.Add(new Base("warning"));
            this.LogGeneral.Add(new Base("notice"));
            this.LogGeneral.Add(new Base("information"));
            this.LogGeneral.Add(new Base("debug"));
            this.LogGeneral.Add(new Base("trace"));
            this.LogGeneral.Add(new Base("all"));

            this.LogMiner.Add(new Base("off"));
            this.LogMiner.Add(new Base("fatal"));
            this.LogMiner.Add(new Base("critical"));
            this.LogMiner.Add(new Base("error"));
            this.LogMiner.Add(new Base("warning"));
            this.LogMiner.Add(new Base("notice"));
            this.LogMiner.Add(new Base("information"));
            this.LogMiner.Add(new Base("debug"));
            this.LogMiner.Add(new Base("trace"));
            this.LogMiner.Add(new Base("all"));

            this.LogNonceSubmitter.Add(new Base("off"));
            this.LogNonceSubmitter.Add(new Base("fatal"));
            this.LogNonceSubmitter.Add(new Base("critical"));
            this.LogNonceSubmitter.Add(new Base("error"));
            this.LogNonceSubmitter.Add(new Base("warning"));
            this.LogNonceSubmitter.Add(new Base("notice"));
            this.LogNonceSubmitter.Add(new Base("information"));
            this.LogNonceSubmitter.Add(new Base("debug"));
            this.LogNonceSubmitter.Add(new Base("trace"));
            this.LogNonceSubmitter.Add(new Base("all"));

            this.LogPlotReader.Add(new Base("off"));
            this.LogPlotReader.Add(new Base("fatal"));
            this.LogPlotReader.Add(new Base("critical"));
            this.LogPlotReader.Add(new Base("error"));
            this.LogPlotReader.Add(new Base("warning"));
            this.LogPlotReader.Add(new Base("notice"));
            this.LogPlotReader.Add(new Base("information"));
            this.LogPlotReader.Add(new Base("debug"));
            this.LogPlotReader.Add(new Base("trace"));
            this.LogPlotReader.Add(new Base("all"));

            this.LogPlotVerifier.Add(new Base("off"));
            this.LogPlotVerifier.Add(new Base("fatal"));
            this.LogPlotVerifier.Add(new Base("critical"));
            this.LogPlotVerifier.Add(new Base("error"));
            this.LogPlotVerifier.Add(new Base("warning"));
            this.LogPlotVerifier.Add(new Base("notice"));
            this.LogPlotVerifier.Add(new Base("information"));
            this.LogPlotVerifier.Add(new Base("debug"));
            this.LogPlotVerifier.Add(new Base("trace"));
            this.LogPlotVerifier.Add(new Base("all"));

            this.LogServer.Add(new Base("off"));
            this.LogServer.Add(new Base("fatal"));
            this.LogServer.Add(new Base("critical"));
            this.LogServer.Add(new Base("error"));
            this.LogServer.Add(new Base("warning"));
            this.LogServer.Add(new Base("notice"));
            this.LogServer.Add(new Base("information"));
            this.LogServer.Add(new Base("debug"));
            this.LogServer.Add(new Base("trace"));
            this.LogServer.Add(new Base("all"));

            this.LogSession.Add(new Base("off"));
            this.LogSession.Add(new Base("fatal"));
            this.LogSession.Add(new Base("critical"));
            this.LogSession.Add(new Base("error"));
            this.LogSession.Add(new Base("warning"));
            this.LogSession.Add(new Base("notice"));
            this.LogSession.Add(new Base("information"));
            this.LogSession.Add(new Base("debug"));
            this.LogSession.Add(new Base("trace"));
            this.LogSession.Add(new Base("all"));

            this.LogSocket.Add(new Base("off"));
            this.LogSocket.Add(new Base("fatal"));
            this.LogSocket.Add(new Base("critical"));
            this.LogSocket.Add(new Base("error"));
            this.LogSocket.Add(new Base("warning"));
            this.LogSocket.Add(new Base("notice"));
            this.LogSocket.Add(new Base("information"));
            this.LogSocket.Add(new Base("debug"));
            this.LogSocket.Add(new Base("trace"));
            this.LogSocket.Add(new Base("all"));

            this.LogWallet.Add(new Base("off"));
            this.LogWallet.Add(new Base("fatal"));
            this.LogWallet.Add(new Base("critical"));
            this.LogWallet.Add(new Base("error"));
            this.LogWallet.Add(new Base("warning"));
            this.LogWallet.Add(new Base("notice"));
            this.LogWallet.Add(new Base("information"));
            this.LogWallet.Add(new Base("debug"));
            this.LogWallet.Add(new Base("trace"));
            this.LogWallet.Add(new Base("all"));

            this.CPUInstSet.Add(new Base("AUTO"));
            this.CPUInstSet.Add(new Base("SSE2"));
            this.CPUInstSet.Add(new Base("SSE4"));
            this.CPUInstSet.Add(new Base("AVX"));
			this.CPUInstSet.Add(new Base("AVX2"));

            this.ProcessorType.Add(new Base("CPU"));
            this.ProcessorType.Add(new Base("CUDA"));
            this.ProcessorType.Add(new Base("OPENCL"));

            DataContext = this;
        }

        private void Button_Click(object sender, RoutedEventArgs ex)
        {
            this.Config["mining"]["urls"]["submission"] = (chPool.IsChecked.HasValue && chPool.IsChecked.Value) ?  wallet.Text : miningInfo.Text;
            this.Config["mining"]["urls"]["miningInfo"] = miningInfo.Text;
            this.Config["mining"]["urls"]["wallet"] = wallet.Text;

            this.Config["mining"]["bufferChunkCount"] = bufferChunkCount.Text;
			this.Config["mining"]["getMiningInfoInterval"] = getMiningInfoInterval.Text;
			this.Config["mining"]["gpuDevice"] = gpuDevice.Text;

			this.Config["mining"]["gpuPlatform"] = gpuPlatform.Text;
			this.Config["mining"]["intensity"] = intensity.Text;
			this.Config["mining"]["maxBufferSizeMB"] = maxBufferSizeMB.Text;

			this.Config["mining"]["maxPlotReaders"] = maxPlotReaders.Text;
			this.Config["mining"]["submissionMaxRetry"] = submissionMaxRetry.Text;
			this.Config["mining"]["submitProbability"] = submitProbability.Text;

			this.Config["mining"]["targetDeadline"] = targetDeadline.Text;
			this.Config["mining"]["timeout"] = timeout.Text;
			this.Config["mining"]["wakeUpTime"] = wakeUpTime.Text;

			this.Config["mining"]["walletRequestRetryWaitTime"] = walletRequestRetryWaitTime.Text;
			this.Config["mining"]["walletRequestTries"] = walletRequestTries.Text;

			this.Config["mining"]["rescanEveryBlock"] = (chRescanEveryBlock.IsChecked.HasValue && chRescanEveryBlock.IsChecked.Value) ? "true" : "false";
			this.Config["mining"]["useInsecurePlotfiles"] = (chUseInsecurePlotfiles.IsChecked.HasValue && chUseInsecurePlotfiles.IsChecked.Value) ? "true" : "false";

			this.Config["mining"]["benchmark"]["active"] = (chBenchmark.IsChecked.HasValue && chBenchmark.IsChecked.Value) ? "true" : "false";

			this.Config["webserver"]["start"] = (chStart.IsChecked.HasValue && chStart.IsChecked.Value) ? "true" : "false";
			this.Config["webserver"]["activeConnections"] = activeConnections.Text;

			this.Config["webserver"]["forwardMinerNames"] = (chForwardMinerNames.IsChecked.HasValue && chForwardMinerNames.IsChecked.Value) ? "true" : "false";
			this.Config["webserver"]["cumulatePlotsizes"] = (chCumulatePlotsizes.IsChecked.HasValue && chCumulatePlotsizes.IsChecked.Value) ? "true" : "false";
			this.Config["webserver"]["calculateEveryDeadline"] = (chCalculateEveryDeadile.IsChecked.HasValue && chCalculateEveryDeadile.IsChecked.Value) ? "true" : "false";

			this.Config["webserver"]["url"] = url.Text;
			this.Config["webserver"]["credentials"]["pass"] = pass.Text;
			this.Config["webserver"]["credentials"]["user"] = user.Text;

			this.Config["logging"]["logfile"] = (chLogfile.IsChecked.HasValue && chLogfile.IsChecked.Value) ? "true" : "false";
			this.Config["logging"]["path"] = txtFirst.Text;

			this.Config["logging"]["useColors"] = (chuseColors.IsChecked.HasValue && chuseColors.IsChecked.Value) ? "true" : "false";
			this.Config["logging"]["progressBar"]["fancy"] = (chFancy.IsChecked.HasValue && chFancy.IsChecked.Value) ? "true" : "false";
			this.Config["logging"]["progressBar"]["steady"] = (chSteady.IsChecked.HasValue && chSteady.IsChecked.Value) ? "true" : "false";

            this.Config["logging"]["config"] = cbConfig.Text;
            this.Config["logging"]["general"] = cbGeneral.Text;
            this.Config["logging"]["miner"] = cbGeneral.Text;

            string executableLocation = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
            string path = Path.Combine(executableLocation, "mining.conf");

			var Settings = new JsonSerializerSettings() { Formatting = Formatting.Indented };
			File.WriteAllText(path, JsonConvert.SerializeObject(Config, Settings));
            ;
        }
    }
}
