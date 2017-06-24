declare module JSONS {
    export interface ConfigObject {
        bufferSize: string;
        maxPlotReaders: number;
        miningInfoUrl: string;
        miningIntensity: number;
        poolUrl: string;
        submissionMaxRetry: string;
        targetDeadline: string;
        timeout: number;
        totalPlotSize: string;
        type: string;
        walletUrl: string;
    }
}