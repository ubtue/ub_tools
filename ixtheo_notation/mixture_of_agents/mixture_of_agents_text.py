# Advanced Mixture-of-Agents example – 3 layers
import asyncio
import json
import os
import sys
import together
import record_util
from together import AsyncTogether, Together

client = Together()
async_client = AsyncTogether()

def Usage():
    print("Usage: " + sys.argv[0] + " ppn")
    sys.exit(1)

with open(record_util.config.get("Mixture of Agents", "ixtheo_notations_file")) as f:
    ixtheo_classes = f.read()


def GetSystemPrompt():
    return f"""You are a classifier that assigns the best matching labels from a classification scheme to bibliographic records. Pay special attention
             to the keywords. If additional information like abstracts, TOCs or fulltext summarizations are given, include them.
             The taxonomy is hierarchical, so check whether the main classes apply. However, one letter classes are forbidden in the final result.
             Return a JSON object with the class and its description. The classes are
             {ixtheo_classes}
    """


def GetUserPrompt(ppn):
#   print(json.dumps(record_util.GetRecordDataWithTOC(ppn)))
   print(json.dumps(record_util.GetRecordDataWithTOCWithoutIxTheoNotation(ppn)))
#   return  """Determine the classes for the following record: """ + json.dumps(record_util.GetRecordDataWithTOC(ppn))  + """Give an explanation an meticulously check that you are correct"""
   return  """Determine the classes for the following record: """ + json.dumps(record_util.GetRecordDataWithoutIxTheoNotation(ppn))  + """Give an explanation an meticulously check that you are correct"""

reference_models = []
reference_models.append(record_util.config.get("Mixture of Agents", "reference_model"))
aggregator_model = "deepseek-ai/DeepSeek-R1"
aggregator_model = record_util.config.get("Mixture of Agents", "aggregator_model")
aggregator_system_prompt = f"""You habe been provided with different suggestions how to assing the ixTheo classification to a record. Meticulouly check the answers and come to a final conclusison which classes are to be assigned and return a JSON object the assigned classes. Pay special attention whether the one letter main classes apply. The classes are: {ixtheo_classes}. Be careful and concise"""

layers = 2

def getFinalSystemPrompt(system_prompt, results):
    """Construct a system prompt for layers 2+ that includes the previous responses to synthesize."""
    return (
        system_prompt
    )

async def run_llm(model, prev_response=None):
    #print("Entering run llm")
    """Run a single LLM call with a model while accounting for previous responses + rate limits."""
    for sleep_time in [1, 2, 4]:
        try:
            messages = (
                [
                    {
                        "role": "system",
                        "content": getFinalSystemPrompt(
                            aggregator_system_prompt, ""
                        ),
                    },
                    {"role": "user", "content": prev_response},
                ]
                if prev_response
                else [ {"role": "system", "content": GetSystemPrompt() },
                       {"role": "user", "content": user_prompt}
                     ]
            )
            response = await async_client.chat.completions.create(
                model=model,
                messages=messages,
                temperature=0.1,
                #max_tokens=8192,
                max_tokens=16384,
            )
            #print("Model: ", model)
            #print("response: " + str(response))
            break
        except together.error.RateLimitError as e:
            print(e)
            await asyncio.sleep(sleep_time)
    return response.choices[0].message.content


def AssembleResults(results):
    return "\n".join([f"{i+1}. {str(element)}" for i, element in enumerate(results)])

async def main():
    if len(sys.argv) != 2:
       Usage()
 
    global user_prompt
    user_prompt = GetUserPrompt(sys.argv[1])
    """Run the main loop of the MOA process."""
    results = await asyncio.gather(*[run_llm(model) for model in reference_models])
    print("<reference_results>" + str(results) + "</reference_results>")

    for _ in range(1, layers - 1):
        results = await asyncio.gather(
            *[run_llm(model, prev_response=results) for model in reference_models]
        )
        print("Final results: " + str(results))

    finalStream = client.chat.completions.create(
        model=aggregator_model,
        messages=[
            {
                "role": "system",
                "content": getFinalSystemPrompt(aggregator_system_prompt, None),
            },
            {"role": "user", "content": AssembleResults(results) },
        ],
        temperature=0.1,
        #max_tokens=8192,
        max_tokens=16384,
        stream=True,
    )
    for chunk in finalStream:
        print(chunk.choices[0].delta.content or "", end="", flush=True)

asyncio.run(main())
