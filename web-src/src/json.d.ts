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


    export interface NewBlockObject {
        baseTarget: number;
        bestDeadlines: number[][];
        bestOverall: string;
        bestOverallNum: number;
        block: number;
        blocksMined: number;
        blocksWon: number;
        deadlinesAvg: string;
        deadlinesConfirmed: number;
        gensigStr: string;
        scoop: number;
        time: string;
        type: string;
    }


        export interface LastWinnerObject {
        address: string;
        name: string;
        numeric: number;
        type: string;
    }
}