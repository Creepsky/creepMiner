declare module JSONS {
    export interface ConfigObject {
        bufferSize: string;
        maxPlotReaders: number;
        miningInfoUrl: string;
        miningIntensity: number;
        poolUrl: string;
        submissionMaxRetry: string;
        targetDeadline: string;
        targetDeadlineText: string;
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


    export interface NonceObject {
        account: string;
        accountId: number;
        deadline: string;
        deadlineNum: number;
        nonce: number;
        plotfile: string;//
        time: string;
        type: string;
    }

    export interface PlotDirObject {
        nonces: Array<JSONS.NonceObject>;
        dir: string;//
        type: string;
        value: number;
        closed: boolean;
    }

}